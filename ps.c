#include "types.h"
#include "stat.h"
#include "user.h"

int number_of_processes = 10;

int
main(int argc, char *argv[])
{
    // struct proc *prcs;
    printf(1,"PID Priority State r_time w_time n_run cur_q q0 q1 q2 q3 q4\n");

    // for(prcs=ptable.proc;prcs<&ptable.proc[NPROC];prcs++)
    // {
    //     int pid = prcs->pid;
    //     if(getpinfo(pid,&p)<0)
    //     {
    //         printf(2, "ps: Error: Process with pid %d does not exist.\n", pid);
    //         exit();
    //     }
    //      printf(1,"%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p.pid,p.priority,p.state,p.runtime,p.wtime,p.num_run,p.current_queue,p.ticks[0],p.ticks[1],p.ticks[2],p.ticks[3],p.ticks[4]);
    // printf(1, "pid %d:\nTotal running time: %d ticks\nNumber of scheduler selections: %d\nCurrent queue: %d\nNumber of running ticks per queue: %d %d %d %d %d\n\n", p.pid, p.runtime, p.num_run, p.current_queue, p.ticks[0], p.ticks[1], p.ticks[2], p.ticks[3], p.ticks[4]);
    // }
    if(ps()<0)
    {
        printf(2,"Error in ps");
        exit();
    }
    exit();
} 
