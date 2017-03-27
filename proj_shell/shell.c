#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<string.h>
#define buf_len 1024

int main(int argc, char *argv[]){
    char line[buf_len]; // get command line
    char** arg;    // user command	
	arg = (char**)malloc(sizeof(char*)*50);
    int i; 
 	if(argc == 1)
	{	   
        while(1)
    	{
        	printf("prompt> "); // print shell prompt

        	if(!fgets(line,buf_len, stdin)) //get command
           		 break;
       
        size_t length = strlen(line);
        if (line[length - 1] == '\n')
            line[length - 1] = '\0';
    
        if(strcmp(line, "quit") == 0)   //if command is quit->exit
            exit(0);

        char *token;    // split the command into a string
        int cnt = 0;
        token = strtok(line,";");
	    arg[0] = token;
        while ((token = strtok(NULL, ";")))
        {
                cnt++;
                arg[cnt] = token;
        }

	    char* str;
	    char*** com;
        com = (char***)malloc(sizeof(char**)*30);
        
        for (i=0;i<30;i++) 
            com[i] = (char**)malloc(sizeof(char*)*30);
	    
        for (i=0;i<=cnt;i++)
        {
                int cnt2=0;
                str = arg[i];
                token = strtok(str," ");
                com[i][0] = token;
                while ((token = strtok(NULL, " ")))
                {
                    cnt2++;
                    com[i][cnt2] = token;
                }
                com[i][cnt2+1] = NULL;
         }
 
    
        int pid; //fork
        for (i=0;i<=cnt;i++)
        {
                if ( (pid=fork() ) == 0)
                {  
                    execvp(com[i][0],com[i]);
                }
                else if (pid > 0) 
                { 
                    pid = wait(NULL);
                }
                else
                { 
                    perror("Failed to fork()!!\n");
                }
            }
        }
    }
  
    else if(argc == 2) // batch mode
    {
        FILE* fp = fopen(argv[1],"r+");
        char line[buf_len];
        while(fgets(line,sizeof(line),fp)!=NULL)
	    {	    
  	        int cnt3 = 0;
            char **cmd;
            char *ptr;
         
            cmd = (char**)malloc(sizeof(char*)*50);
            strcat(line, ";");
            ptr = strtok(line, ";");
            cmd[0] = ptr;
            while ((ptr = strtok(NULL, ";"))){
                cnt3++;
                cmd[cnt3] = ptr;
            }
          
            strncpy(cmd[cnt3],cmd[cnt3],strlen(cmd[cnt3])-1);

            cmd[cnt3][strlen(cmd[cnt3])-1] = '\0';
         
            char *str;
            char ***arg;
            
            arg = (char***)malloc(sizeof(char**)*30);
            for (i=0;i<30;i++)
                arg[i] = (char**)malloc(sizeof(char*)*30);
            
            for (i=0;i<=cnt3;i++)
            {
                int cnt4 = 0;
                str = cmd[i];
                ptr = strtok(str," ");
                arg[i][0] = ptr;
                while ((ptr = strtok(NULL, " ")))
                {
                    cnt4++;
                    arg[i][cnt4] = ptr;
                }
                arg[i][cnt4+1] = NULL;
            }
            
            int pid;
            for (i=0;i<=cnt3;i++)
            {
                if ( (pid=fork() ) == 0)
                {   
                    execvp(arg[i][0],arg[i]);
                }
                else if (pid > 0) 
                { 
                    pid = wait(NULL);
                }
                else
                {
                    perror("Faild to fork()!!\n");
                }
            }
            
        }
        fclose(fp);
    }
}
