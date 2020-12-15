#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
    initlock(&ptable.lock, "ptable");

#if SCHEDULER == MLFQ
    acquire(&ptable.lock);
    for (int i = 0; i < NPROC; i++) { avail_alloc_nodes[i].available = 0; }
    for (int i = 0; i < 5; i++) { queues[i] = 0; }
    release(&ptable.lock);
#endif
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}
void proc_push(struct proc *p, int q)
{
  queues[q] = push(queues[q],p);
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
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

  release(&ptable.lock);

  p->ctime = ticks;
  p->rtime = 0;
  p->etime = 0;
  p->wtime = 0;
  p->age = ticks;
  p->n_run = 0;
  for (int i = 0; i < 5; i++) 
  {
    p->queue_ticks[i] = -1;
  }
  p->priority = 60;
  p->cur_q = 0;
  p->penalty = -1;
  p->timeslices = 0;
  p->cur_timeslices = 0;

#if SCHEDULER == PBS
  p->priority = 60;
  p->penalty = 0;
  p->timeslices = 0;
  p->cur_timeslices = 0;

#elif SCHEDULER == MLFQ
  p->priority = 60;
  p->penalty = 0;
  p->timeslices = 0;
  p->cur_timeslices = 0;

  for (int i = 0; i < 5; i++) 
  {
    p->queue_ticks[i] = 0;
  }

#elif SCHEDULER == FCFS
  p->cur_q=-1;
#endif

  // Allocate kernel stack.
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

  return p;
}

int rtimeticks() 
{
  struct proc *prc;
  int cnt=0;
  acquire(&ptable.lock);
  for(prc=ptable.proc;prc<&ptable.proc[NPROC];prc++)
  {
    if(prc->state == RUNNING)
    {
      prc->wtime=0;
      prc->rtime++;
      cnt++;
    }
  }
  release(&ptable.lock);
  return cnt;
}

void changeflag() 
{
    acquire(&ptable.lock);
    myproc()->queue_ticks[myproc()->cur_q]++;
    myproc()->penalty = 1;
    release(&ptable.lock);
}

void update_proc_timeslice() 
{
    acquire(&ptable.lock);
    myproc()->cur_timeslices++;
    myproc()->queue_ticks[myproc()->cur_q]++;
    release(&ptable.lock);
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
#if SCHEDULER == MLFQ
  proc_push(p,0);
  // queues[0] = push(queues[0], p);
#endif
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
#if SCHEDULER == MLFQ
proc_push(np,0);
  // queues[0] = push(queues[0], np); 
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

 
  // Set the end time of the process as the current CPU cycle
  curproc->etime = ticks;

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

int
waitx(int* wtime, int* rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        *rtime = p->rtime;
        *wtime = p->etime-(p->ctime+p->rtime);
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int 
set_priority(int new_priority,int pid) 
{
  int old_priority=-69;
  if(new_priority<0) { return old_priority;}
  if(new_priority>100) { return old_priority; }
  // cprintf("callled pid: %d priority:%d\n",pid,new_priority);

  struct proc *p;
  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
  {
    if(p->pid == pid)
    {
      old_priority = p->priority;
      p->priority = new_priority;
      if(old_priority!=new_priority)
      p->timeslices=0;
      // cprintf("pid: %d priority:%d\n",p->pid,p->priority);
    }
  }
  release(&ptable.lock);
  if(old_priority==-69)
  {
    return old_priority;
  }
  if(old_priority<=new_priority)
  return old_priority;
  else
  {
    yield();
  }
  return old_priority;
}

//PAGEBREAK: 42
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
  struct cpu *c = mycpu();
  c->proc = 0;

  #if SCHEDULER == RR
    for(;;){
      // Enable interrupts on this processor.
      sti();

      // Loop over process table looking for process to run.
      acquire(&ptable.lock);
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        p->n_run++;
        switchuvm(p);
        p->state = RUNNING;
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&ptable.lock);
    }
  #elif SCHEDULER == FCFS
    while (1) {
      // Enable interrupts on this processor - yielding disabled for FCFS
      sti();
      // Loop over process table looking for the process with earliest creation time to run
      int min_limit = ticks + 200; 
      struct proc* chosenproc = 0;
      int found=0;
      acquire(&ptable.lock);
      for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
      {
        if(p->state == RUNNABLE) 
        {
          if (p->ctime < min_limit) 
          {
            min_limit = p->ctime;
            chosenproc = p;
            found=1;
          }
        }
        else
        continue;
      }
      if(!found)
      {
        release(&ptable.lock);
        continue;
      }

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = chosenproc;
      chosenproc->n_run++;
      switchuvm(chosenproc);
      chosenproc->state = RUNNING;

      swtch(&(c->scheduler), chosenproc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      release(&ptable.lock);
    }
#elif SCHEDULER == PBS
  while (1) {
    // Enable interrupts on this processor
    sti();
    // Loop over process table looking for the process with the highest priority (and least timeslices to break ties)
    struct proc *chosenproc = 0;
    int found=0;

    int max_limit = 101;
    int min_limit = ticks + 200; 
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
    {
      if (p->state == RUNNABLE) 
      {
        if(p->priority == max_limit && p->timeslices < min_limit)
        {
          {
            min_limit = p->timeslices;
            chosenproc = p;
            found=1;
          }
        }
        else if (p->priority < max_limit) 
        {
          found=1;
          max_limit = p->priority;
          min_limit = p->timeslices;
          chosenproc = p;
        } 
      }
    }
    
    if(!found) 
    {
      release(&ptable.lock);
      continue;
    }

    chosenproc->timeslices++;
    chosenproc->n_run++;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = chosenproc;
    switchuvm(chosenproc);
    chosenproc->state = RUNNING;

    swtch(&(c->scheduler), chosenproc->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    release(&ptable.lock);
  }


#elif SCHEDULER == MLFQ
  while (1) {
    // Enable interrupts on this processor
    sti();

    acquire(&ptable.lock);

    // Age the processes
    int i=1;
    while(i<5)
    {
      proc_queue_change(WAIT_LIMIT, &queues[i], &queues[i - 1]);
      i++;
    }

    // Loop through the queues to find a process to run
    p = 0;
    i=0;
    while(i<5)
    {
      if (countnodes(queues[i]) == 0) 
      {
        i++;
        continue;
      }
      p = queues[i]->prc;
      queues[i] = pop(queues[i]);
      break;
    }

    if (p == 0 || p->state != RUNNABLE) 
    {
      release(&ptable.lock);
      continue;
    }

    p->cur_timeslices++;
    p->n_run++;
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    c->proc = 0;

    // Back from p
    if (p != 0 && p->state == RUNNABLE) 
    {
      if(p->penalty != 0) 
      {
        if (p->cur_q != NUM_QUEUES-1) { p->cur_q++;
            // cprintf("PLOT %d %d %d\n",p->pid,p->cur_q,ticks);  
         }
        p->cur_timeslices = 0;
        p->penalty = 0;
        p->age = ticks;
        p->wtime=0;
        proc_push(p,p->cur_q);
      } 
      else 
      {
        p->cur_timeslices = 0;
        p->age = ticks;
        p->wtime=0;
        proc_push(p,p->cur_q);
      }
    }

    release(&ptable.lock);
  }
#endif

}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
#if SCHEDULER == MLFQ
  proc_push(p,p->cur_q);
      // queues[p->cur_q] = push(queues[p->cur_q], p);
      p->age = ticks;
      p->cur_timeslices = 0;
#endif
    }
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
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
#if SCHEDULER == MLFQ
proc_push(p,p->cur_q);
        // queues[p->cur_q] = push(queues[p->cur_q], p);
        p->cur_timeslices = 0;
        p->age = ticks;
#endif
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
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
#if SCHEDULER == MLFQ
    cprintf("%d %s %s QUEUE: %d TIMESLICES %d", p->pid, state, p->name, p->cur_q, p->timeslices);
#else
    cprintf("%d %s %s", p->pid, state, p->name);
#endif
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
int ps(void)
{
  struct proc *process[NPROC];
  struct proc *p;
  int k=0;
  cprintf("PID\tPriority\tState\tr_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");

  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
  {
    // int pid = p->pid;
    // if(getpinfo(pid,&process[k])<0)
    // {
    //   return -1;
    // }
    if(p->pid==0)
    continue;
    process[k]=p;
    // char *stte = (char*)malloc(100*sizeof(char));
    char *stte;
    switch(p->state)
    {
      case UNUSED:
      stte="UNUSED";
      break;
      case EMBRYO:
      stte="EMBRYO";
      break;
      case SLEEPING:
      stte="SLEEPING";
      break;
      case RUNNABLE:
      stte="RUNNABLE";
      break;
      case RUNNING:
      stte="RUNNING";
      break;
      case ZOMBIE:
      stte="ZOMBIE";
      break;
      default: stte="UNUSED";
      break;
    }
    if(p->state == RUNNING)
    cprintf("%d\t%d\t%s\t\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,stte,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // cprintf("%d\t%d\t%s\t\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,stte,process[k]->rtime,ticks-process[k]->age,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    else
    cprintf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,stte,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // cprintf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,stte,process[k]->rtime,ticks-process[k]->age,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);

    // else if(p->state==EMBRYO)
    // cprintf("%d\t%dEMBRYO\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // else if(p->state==SLEEPING)
    // cprintf("%d\t%d\tSLEEPING\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // else if(p->state==RUNNABLE)
    // cprintf("%d\t%d\tRUNNABLE\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // else if(p->state==RUNNING)
    // cprintf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,stte,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    // else if(p->state==ZOMBIE)
    // cprintf("%d\t%d\tZOMBIE\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",process[k]->pid,process[k]->priority,process[k]->rtime,process[k]->wtime,process[k]->n_run,process[k]->cur_q,process[k]->queue_ticks[0],process[k]->queue_ticks[1],process[k]->queue_ticks[2],process[k]->queue_ticks[3],process[k]->queue_ticks[4]);
    k++;
  }
  // struct proc* p;
  // acquire(&ptable.lock);
//   printf(1,"ps:\n");
//   printf(1,"PID\tPriority\tState\tr_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
//   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
//   {

//     printf(1,"%d %d %s %d %d %d %d %d %d %d %d %d\n",p->pid,p->priority,
//               p->state,p->cputime,p->wtime,p->n_run,p->cur_q,
//               p->q_ticks[0],p->q_ticks[1],p->q_ticks[2],p->q_ticks[3],p->q_ticks[4]);
//   }
  release(&ptable.lock);
  return 0;
}
int 
wtimeticks()
{
  int cnt=0;
  struct proc *p;
  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
  {
    if(p->state == RUNNABLE)
    {
      p->wtime++;
      cnt++;
    }
  }
  release(&ptable.lock);

  return cnt;
}