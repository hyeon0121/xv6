#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define LARGE_NUM 10000

typedef unsigned int thread_t;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

//my stride
struct stride {
    struct proc* proc;
    int ticket;
    int stride;
    int pass;
};
struct stride stride_t[NPROC];
struct stride isMLFQ;
struct stride* next_stride = &isMLFQ;

//my thread
int nexttid = 1;

//my mlfq 
struct proc *queue[3][NPROC];
int front[3] = {0,0,0};
int limit[3] = {5,10,20};
int count = 0;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

//Modified proj2 MLFQ
int Enqueue(struct proc* p, int prio){
    int val = 0;
    int isfront = front[prio];
    val = isfront;
    do{
        if(queue[prio][val] == 0)
            break; 
        val = (val + 1)%NPROC;
      //  if(val == isfront) break;
    }while(val != isfront);

    queue[prio][val] = p;
    return 1;
}
int Dequeue(struct proc* p){
    int i;
    for(i=0;i<NPROC; i++){
        if(queue[p->priority][i]->pid == p->pid){
            queue[p->priority][i] = 0;
            return 1;
        }
    }
    return 0;
}
int priority_boost(){
    int i, j;
    struct proc* p;

    for(i=0;i<3;i++){
        for(j=0;j<NPROC;j++){
            queue[i][j] = 0;
        }
        front[i] = 0;
    }

    acquire(&ptable.lock);
    
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != UNUSED && p->pstride == &isMLFQ){
            p->priority = 0;
            p->currtick = 0;
            Enqueue(p,0);
        }
    
    }
    release(&ptable.lock);

    return 1;
}
struct proc* select_proc(){
    int val ,i;
    struct proc* p;
    for(i=0;i<3;i++){
        val = front[i];
        do{
           p=queue[i][front[i]];
           if(p != 0){
              if(p->state == RUNNABLE)
                    return p;
           }
                
           front[i] = (front[i]+1) % NPROC;
           //if(tmp == front[i]) break;
        
        }while(val!=front[i]);
    }
    return 0;
}
void mlfq_implement(void){
    if(proc->pstride != &isMLFQ)
        return;
    proc->currtick++;
    count++;
    if(count >= 100){
        count = 0;
        priority_boost();
        return;
    }

    switch (proc->priority){
        case 0:
            if(proc->currtick >= limit[proc->priority]){
                Dequeue(proc);
                front[proc->priority] = (front[proc->priority]+1) % NPROC;
                Enqueue(proc,++(proc->priority));
                proc->currtick = 0; //initial tick
            }
            break;
        case 1:
            if(proc->currtick >= limit[proc->priority]){
                Dequeue(proc);
                front[proc->priority] = (front[proc->priority]+1) % NPROC;
                Enqueue(proc,++(proc->priority));
                proc->currtick = 0; //initial tick
            }
            break;
        case 2:
            if(proc->currtick >= limit[proc->priority]){
                proc->currtick = 0; //initial tick
                front[proc->priority] = (front[proc->priority]+1) % NPROC;
            }
            break;
    }
}
//my stride code
int set_cpu_share(int n) {
    int total_tick = 0;
    struct stride *s;
    if(n <= 0) return -1;
    
    for(s = stride_t; s < &stride_t[NPROC]; s++ ){
        if(s->proc != 0)
            total_tick = total_tick +  s->ticket;
    }
    total_tick = total_tick + n;
    
    if(total_tick > 80) return -1;
    for(s = stride_t; s < &stride_t[NPROC]; s++){
        if(s->proc != 0){
            s->pass = 0;
        }
    }
    for(s = stride_t; s < &stride_t[NPROC]; s++){
        if(s->proc == 0){
            break;
        }
    }

    s->pass = 0;
    s->ticket = n;
    s->stride = LARGE_NUM / n;
    s->proc = proc;

    proc->pstride = s;

    isMLFQ.pass = 0;
    isMLFQ.ticket = 100 - total_tick;
    isMLFQ.stride = LARGE_NUM / isMLFQ.ticket;
    isMLFQ.proc = 0;

    Dequeue(proc);
    return n;
}
struct stride* select_min_stride(){
    struct stride *s;
    struct stride *min;

    min = &isMLFQ;
    for(s = stride_t; s < &stride_t[NPROC]; s++){
        if(s->proc != 0)
            if(min->pass > s->pass){
                min = s;
            }
    } 
    return min;
}
int Destride(struct proc* p){
    struct stride *s;
    
    for(s = stride_t; s < &stride_t[NPROC]; s++){
        if(s->proc != 0)
            if(s->proc->pid == p->pid){
                s->proc = 0;
                p->pstride = 0;
                return -1;
            }
    }
    return 0;
}
void stride_implement(){
    struct stride *s;

    proc->pstride->pass += proc->pstride->stride;
    next_stride = select_min_stride();

    if(proc->pstride->pass > LARGE_NUM){
        if(proc->pstride != next_stride){
            isMLFQ.pass = 0;
            for(s = stride_t; s < &stride_t[NPROC]; s++){
                if(s->proc != 0)
                    s->pass = 0;
            }
        }
    }
    return;
}
void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  //p->priority = 0;
  //p->tick = 0;
  
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  
  //initial variable
  p->currtick = 0;
  p->priority = 0;
  p->pstride = &isMLFQ;
  Enqueue(p,0);
  p->isthread = 0;
  p->thread_id = 0;
  p->retval = 0;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  pid = np->pid;
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  //struct proc *tmp;
  struct proc *m = ptable.proc;
  //int i, tmp;


  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
	//int prio;
	//for(prio = 0; prio < 3; prio++) {
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->state != RUNNABLE)
                continue;
            if(select_proc() != 0){
                p = select_proc();
            }
            /*for(i=0;i<3;i++){
                tmp = front[i];
                do{
                    p1=queue[i][front[i]];
                    if(p1 != 0){
                        if(p1->state == RUNNABLE)
                            p = p1;
                    }
                
                    front[i] = (front[i]+1) % NPROC;
                }while(tmp!=front[i]);
             }*/

            if(next_stride->proc != 0){
                if(next_stride->proc->state == RUNNABLE)
                    p = next_stride->proc;
            }

            for(; m < &ptable.proc[NPROC]; m++){
                if(m > &ptable.proc[NPROC-1])
                    m = ptable.proc;
                if((m->parent == p && m->thread_id != 0) || m == p)
                {
                    p = m;
                    break;
                }
            }

            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&cpu->scheduler, proc->context);
            switchkvm();
                
            proc = 0;
        }
        release(&ptable.lock);
    }
}
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      
    
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
			 p->state = RUNNABLE;
		}
     
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
  
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
    int i;
    struct proc *np;
  
    //uint sp; 
    uint ustack[3];

    // Allocate process.
    if((np = allocproc()) == 0){
        cprintf("Cannot allocate!\n");
        return -1;
    }
    Dequeue(np);
    np->pstride = 0;
    np->isthread = 1;
    np->pgdir = proc->pgdir;
    /*
    np->thread_id = nexttid++;
    *thread = np->thread_id;
    np->pgdir = proc->pgdir;
    np->sz = proc->sz;
    np->parent = proc;
    np->isthread = 1;
    *np->tf = *proc->tf;

    np->tf->eax = 0;
    np->tf->eip = (uint)start_routine;   

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible.  Use the second as the user stack.

    if((np->sz = allocuvm(np->pgdir, np->sz, np->sz + 2*PGSIZE)) == 0){
        return -1;
    }
    proc->sz = np->sz;
    clearpteu(np->pgdir, (char*)(np->sz - 2*PGSIZE));
    sp = np->sz;

    sp = (sp - (sizeof(arg)));
    if(copyout(np->pgdir,sp,arg,sizeof(arg))<0){
        return -1;
    }

    // Push argument strings, prepare rest of stack in ustack.
    ustack[3] = sp;
    ustack[4] = 0;
    ustack[0] = 0xffffffff;  // fake return PC
    ustack[1] = 1;
    ustack[2] = sp - (1+1)*4;



    sp -= (3+1+1) * 4;
    if(copyout(np->pgdir, sp, ustack, (3+1+1)*4) < 0){
        np->state = ZOMBIE;
        return -1;
    }

    for(i=0;i<NOFILE;i++)
        if(proc->ofile[i])
            np->ofile[i] = filedup(proc->ofile[i]);
    np->cwd = idup(proc->cwd);

    safestrcpy(np->name, proc->name, sizeof(proc->name));

    np->tf->esp = sp;
   
    acquire(&ptable.lock);
    np->state = RUNNABLE;
    release(&ptable.lock);
   
    return 0;*/
    *np->tf = *proc->tf;
    np->parent = proc;

    // Clear %eax
    np->tf->eax = 0;
    for(i=0;i<NOFILE; i++)
        if(proc->ofile[i])
            np->ofile[i] = filedup(proc->ofile[i]);
    np->cwd = idup(proc->cwd);

    safestrcpy(np->name, proc->name, sizeof(proc->name));

    if(growproc(PGSIZE) == -1){
        return -1;
    }

    np->tf->esp = proc->sz;
    np->sz = proc->sz;

    np->tf->eip = (uint)start_routine;

    ustack[0] = 0xffffffff;
    ustack[1] = (uint)arg;
    ustack[2] = 0;

    np->tf->esp -= 12;

    if(copyout(np->pgdir, np->tf->esp, ustack, 12) < 0){
        return -1;
    }

    np->thread_id = nexttid++;
    *thread = np->thread_id;

    acquire(&ptable.lock);
    np->state = RUNNABLE;
    release(&ptable.lock);
    
    return 0;


}
void
thread_exit(void *retval)
{
    
    struct proc* p;
    int fd;
    proc->retval = retval;
    
    if(proc == initproc)
        panic("init exiting");
    for(fd=0;fd < NOFILE; fd++){
        if(proc->ofile[fd]){
            fileclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(proc->cwd);
    end_op();
    proc->cwd = 0;

    acquire(&ptable.lock);
    wakeup1(proc->parent);

    for(p = ptable.proc; p < &ptable.proc[NPROC];p++){
        if(p->parent == proc){
            p->parent = initproc;
            /*if(p->isthread == 1){
                p->state = UNUSED;
                kfree(p->kstack);
                p->kstack = 0;
            }*/
            if(p->state == ZOMBIE)
                wakeup1(initproc);
        }
    }
    proc->state = ZOMBIE;
    
    if(proc->pstride == &isMLFQ ){
        Dequeue(proc);
    }
    else{
        Destride(proc);
    }
    sched();
    panic("zombie exit");
}
int
thread_join(thread_t thread, void **retval)
{
    struct proc *p;
    int havekids = 0;

    acquire(&ptable.lock);
    
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->thread_id == thread && p->isthread == 1){
                havekids = 1;
                break;
            }
        }
        if( havekids == 0){
            cprintf("No have thread\n");
        }

        while(p->state != ZOMBIE){
            sleep(proc, &ptable.lock);
        }
     
        *retval = p->retval;
        
        deallocuvm(p->pgdir, p->sz, p->sz - PGSIZE);
        
        kfree(p->kstack);
        p->kstack = 0;
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->thread_id = 0;
        p->retval = 0;
        release(&ptable.lock);
                
        return 0;
} 
