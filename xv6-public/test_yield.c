#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char *argv[])
{
    
    int rc = fork();
    while(1)
    {
        if(rc<0)
        {
            printf(1, "fork failed\n");
            }
        else if (rc == 0)
        {
            printf(1, "Child\n");
            yield();
            sleep(100);
            }
        else
        {
            printf(1, "Parent\n");
            yield();
            sleep(100);
            }
                
        }
    }
