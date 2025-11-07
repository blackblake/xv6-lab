#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->priority = 15;  // 默认优先级为15（中等优先级）
  p->wait_time = 0;  // 初始化等待时间
  p->remaining_time = 0;  // 初始化剩余时间片
  p->proc_type = BATCH_PROCESS;  // 默认为批处理进程
  p->queue_level = 2;  // 默认为低优先级队列

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->priority = 0;
  p->remaining_time = 0;
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Simple priority scheduler
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  struct proc *selected = 0;
  int highest_priority = 1000; // Start with a high number
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    // Find the process with highest priority (lowest number)
    selected = 0;
    highest_priority = 1000;
    
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE && p->priority < highest_priority) {
        if(selected != 0) {
          release(&selected->lock);
        }
        selected = p;
        highest_priority = p->priority;
      } else {
        release(&p->lock);
      }
    }
    
    if(selected != 0) {
      // Switch to chosen process
      selected->state = RUNNING;
      c->proc = selected;
      swtch(&c->context, &selected->context);

      // Process is done running for now
      c->proc = 0;
      release(&selected->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

// ==================== Round Robin Queue Operations ====================

// Initialize the round robin queue
void
init_queue(RoundRobinQueue* rq, int quantum)
{
  rq->front = 0;
  rq->rear = 0;
  rq->time_quantum = quantum;
}

// Check if the queue is empty
int
is_empty(RoundRobinQueue* rq)
{
  return rq->front == rq->rear;
}

// Add a process to the round robin queue
void
enqueue(RoundRobinQueue* rq, struct proc* p)
{
  if((rq->rear + 1) % NPROC == rq->front) {
    // Queue is full
    return;
  }
  
  rq->queue[rq->rear] = p;
  rq->rear = (rq->rear + 1) % NPROC;
}

// Remove and return a process from the round robin queue
struct proc*
dequeue(RoundRobinQueue* rq)
{
  if(is_empty(rq)) {
    return 0; // Queue is empty
  }
  
  struct proc* p = rq->queue[rq->front];
  rq->front = (rq->front + 1) % NPROC;
  return p;
}

// Round Robin scheduling algorithm implementation
void
round_robin_schedule(struct proc processes[], int n, int quantum)
{
  RoundRobinQueue rq;
  init_queue(&rq, quantum);
  
  // Add all processes to the queue
  for(int i = 0; i < n; i++) {
    if(processes[i].state == RUNNABLE) {
      processes[i].remaining_time = quantum;
      enqueue(&rq, &processes[i]);
    }
  }
  
  printf("Round Robin Scheduling with quantum %d:\n", quantum);
  
  while(!is_empty(&rq)) {
    struct proc* current = dequeue(&rq);
    if(current == 0) break;
    
    printf("Process %d running for time slice %d\n", current->pid, current->remaining_time);
    
    // Simulate process execution
    // In real implementation, this would be handled by the scheduler
    current->remaining_time = 0;
    current->state = RUNNABLE;
    
    // If process still needs CPU time, add it back to queue
    // This is a simplified version - in reality, we'd check if process is still runnable
    if(current->state == RUNNABLE) {
      current->remaining_time = quantum;
      enqueue(&rq, current);
    }
  }
}

// ==================== Multi-level Queue Operations ====================

// Initialize the multilevel queue
void
init_multilevel_queue(MultiLevelQueue* mlq)
{
  // Initialize each queue with different time quantums
  init_queue(&mlq->high_priority, 2);    // 系统进程：短时间片
  init_queue(&mlq->medium_priority, 4);  // 交互进程：中等时间片
  init_queue(&mlq->low_priority, 8);     // 批处理进程：长时间片
}

// Add a process to a specific queue level
void
enqueue_to_level(MultiLevelQueue* mlq, struct proc* p, int level)
{
  switch(level) {
    case 0: // High priority queue
      enqueue(&mlq->high_priority, p);
      p->queue_level = 0;
      break;
    case 1: // Medium priority queue
      enqueue(&mlq->medium_priority, p);
      p->queue_level = 1;
      break;
    case 2: // Low priority queue
      enqueue(&mlq->low_priority, p);
      p->queue_level = 2;
      break;
    default:
      // Default to low priority
      enqueue(&mlq->low_priority, p);
      p->queue_level = 2;
      break;
  }
}

// Remove and return a process from a specific queue level
struct proc*
dequeue_from_level(MultiLevelQueue* mlq, int level)
{
  struct proc* p = 0;
  switch(level) {
    case 0: // High priority queue
      p = dequeue(&mlq->high_priority);
      break;
    case 1: // Medium priority queue
      p = dequeue(&mlq->medium_priority);
      break;
    case 2: // Low priority queue
      p = dequeue(&mlq->low_priority);
      break;
  }
  return p;
}

// Schedule a process from the multilevel queue (priority-based)
struct proc*
schedule_from_multilevel(MultiLevelQueue* mlq)
{
  struct proc* p = 0;
  
  // Check high priority queue first
  if(!is_empty(&mlq->high_priority)) {
    p = dequeue_from_level(mlq, 0);
    if(p) return p;
  }
  
  // Check medium priority queue
  if(!is_empty(&mlq->medium_priority)) {
    p = dequeue_from_level(mlq, 1);
    if(p) return p;
  }
  
  // Check low priority queue
  if(!is_empty(&mlq->low_priority)) {
    p = dequeue_from_level(mlq, 2);
    if(p) return p;
  }
  
  return 0; // No process available
}

// Classify a process based on its characteristics
void
classify_process(struct proc* p)
{
  // Simple classification based on process name and characteristics
  if(strncmp(p->name, "init", 4) == 0 || strncmp(p->name, "sh", 2) == 0) {
    p->proc_type = SYSTEM_PROCESS;
  } else if(strncmp(p->name, "hello", 5) == 0 || strncmp(p->name, "echo", 4) == 0) {
    p->proc_type = INTERACTIVE_PROCESS;
  } else {
    p->proc_type = BATCH_PROCESS;
  }
}

// Migrate a process between queue levels
void
migrate_process(MultiLevelQueue* mlq, struct proc* p, int from_level, int to_level)
{
  if(from_level == to_level) return;
  
  // Remove from old queue (if it exists)
  // Note: In a real implementation, we'd need to search and remove
  // For simplicity, we assume the process is already dequeued
  
  // Add to new queue
  enqueue_to_level(mlq, p, to_level);
  
  printf("Process %d migrated from level %d to level %d\n", 
         p->pid, from_level, to_level);
}

// Multilevel queue scheduling algorithm
void
multilevel_queue_schedule(struct proc processes[], int n)
{
  MultiLevelQueue mlq;
  init_multilevel_queue(&mlq);
  
  // Classify and add all processes to appropriate queues
  for(int i = 0; i < n; i++) {
    if(processes[i].state == RUNNABLE) {
      classify_process(&processes[i]);
      
      switch(processes[i].proc_type) {
        case SYSTEM_PROCESS:
          enqueue_to_level(&mlq, &processes[i], 0);
          break;
        case INTERACTIVE_PROCESS:
          enqueue_to_level(&mlq, &processes[i], 1);
          break;
        case BATCH_PROCESS:
          enqueue_to_level(&mlq, &processes[i], 2);
          break;
      }
    }
  }
  
  printf("多级队列调度算法:\n");
  printf("==================\n");
  
  int time = 0;
  int completed = 0;
  
  while(completed < n) {
    struct proc* current = schedule_from_multilevel(&mlq);
    if(current == 0) break;
    
    int quantum = 0;
    switch(current->queue_level) {
      case 0: quantum = 2; break;  // High priority
      case 1: quantum = 4; break;  // Medium priority
      case 2: quantum = 8; break;  // Low priority
    }
    
    printf("时间 %d: 进程 %d (类型: %d, 队列: %d) 开始执行\n", 
           time, current->pid, current->proc_type, current->queue_level);
    
    // Simulate process execution
    int execution_time = (current->remaining_time < quantum) ? 
                        current->remaining_time : quantum;
    current->remaining_time -= execution_time;
    time += execution_time;
    
    printf("时间 %d: 进程 %d 执行了 %d 个时间单位\n", 
           time, current->pid, execution_time);
    
    if(current->remaining_time <= 0) {
      // Process completed
      current->state = 2; // COMPLETED
      completed++;
      printf("时间 %d: 进程 %d 完成\n", time, current->pid);
    } else {
      // Process not completed, add back to queue
      current->state = 0; // READY
      current->remaining_time = quantum;
      enqueue_to_level(&mlq, current, current->queue_level);
      printf("时间 %d: 进程 %d 重新加入队列 %d\n", 
             time, current->pid, current->queue_level);
    }
  }
  
  printf("==================\n");
  printf("所有进程调度完成，总时间: %d\n", time);
}
