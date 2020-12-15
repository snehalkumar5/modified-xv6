## ASSIGNMENT - 5
### SNEHAL KUMAR
### 2019101003
### waitx (proc.c)
The waitx system call added to proc.c. It returns value same as that of wait syscall. Calculates the wtime (endtime-creationtime-runtime) and runtime of the process. 
Note: time command is used to call waitx with arguments *wtime,*rtime.
	proc structure is modified to include ctime,rtime,etime etc in proc.h
### ps (ps.c)
The user program ps calls ps system call(proc.c) to display the information about the active processes. Loops through ptable (all processes) and displays the relevant information of each active process.
### setpriority (setPriority.c)
The user program calls set_priority system call (proc.c) to set the new priority of process pid. It takes two arguments (newpriority,pid) and yields if newpriority<oldpriority (higher priority=lower value). It holds value when scheduler is PBS.

### SCHEDULERS
#### First Come First Served (FCFS): 
Loops through the ptable for runnable processes and selects the process with lowest creation time, changes state to running and switches to that process.
#### Priority Based Scheduler (PBS):
Loops through ptable for runnable processes and selects the process with highest priority(lowest priority value). If two processes have same priority, it selects the one with least timeslices (round robin priority). Change state to running and switch context.
Default priority for process=60.
#### Multi Level Feedback Queue:
A queue is implemented to handle the 5 priority queues for MLFQ.
Initially all processes are in queue[0]. Each queue has max timeslice limit. If process timeslices exceed the queue limit, it is demoted to lower priority queue and runs till it finishes process within the limit or demotes and repeats. While the processes are not RUNNING, the wait time increases. To prevent starvation, aging is implemented which promotes the priority of process with high wait time (>35 ticks) . 
