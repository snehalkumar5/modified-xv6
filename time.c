#include "types.h"
#include "fcntl.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char* argv[])
{
    if (argc <= 1) {
        printf(2, "Error in time: Invalid arguments\n");
        exit();
    }
    int pid,wtime,rtime,status;
    pid = fork();
    if (pid < 0) 
    {
        printf(2,"Error in time: Forking error\n");
        exit();
    } 
    else if(pid==0)
    {
        printf(1,"Timing %s\n", argv[1]);
        if(exec(argv[1], argv + 1)<0) 
        {
            printf(2, "Error in time: Execution error\n");
            exit();
        }
    } 
    else 
    {
        status=waitx(&wtime,&rtime);
        printf(1,"Time taken - %s\nProcess ID: %d\nWaiting time: %d\nRunning time: %d\n\n", argv[1], status, wtime, rtime);
        exit();
    }
}