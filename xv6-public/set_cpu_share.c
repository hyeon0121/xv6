#include "types.h"
#include "stat.h"
#include "user.h"
#include "proc.h"

int
set_cpu_share(int x){
    proc->ticket = x;
    proc->pass = 0;
    proc->count -= x;
    if(proc->count < 20){
            printf(1,"MLFQ at least 20 cpu shar!!\n");
            return -1;
    }
    if(proc->ticket > 80){
            printf(1,"STRIDE can not exceed 80 cpu time!!\n");
            return -1;
    }
    proc->stride = 10000/p->ticket;
    return 0;
}
int 
sys_set_cpu_share(void)
{
    int x;
    return set_cpu_share(x);
}
