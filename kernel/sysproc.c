#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

// 引用进程表
extern struct proc proc[NPROC];

// 在内核侧定义与用户态 struct procinfo 相同布局的结构，避免包含 user 头文件
struct kprocinfo {
  int pid;
  int ppid;
  int state;
  uint sz;
  char name[16];
};

struct ksystime {
  uint ticks;
  uint uptime;
};

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_getsystime(void)
{
  struct systime *time;
  if(argaddr(0, (uint64*)&time) < 0)
    return -1;
  if(time == 0)
    return -1;

  struct ksystime kt;
  acquire(&tickslock);
  kt.ticks = ticks;
  kt.uptime = ticks / 100; // 假设100 ticks = 1秒
  release(&tickslock);

  if(copyout(myproc()->pagetable, (uint64)time, (char*)&kt, sizeof(kt)) < 0)
    return -1;
  return 0;
}

uint64
sys_setpriority(void)
{
  int pid, prio;
  if(argint(0, &pid) < 0 || argint(1, &prio) < 0)
    return -1;
  if(prio < 0 || prio > 10)
    return -1;

  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->priority = prio;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_getprocinfo(void)
{
  struct procinfo *info;
  struct proc *p = myproc();
  if(argaddr(0, (uint64*)&info) < 0)
    return -1;
  if(info == 0)
    return -1;

  // 填充信息
  // 注意：procinfo 定义在 user/user.h，字段与内核结构对应
  // 这里直接写用户空间指针是不安全的，需使用 copyout
  struct kprocinfo kinfo;
  kinfo.pid = p->pid;
  kinfo.ppid = p->parent ? p->parent->pid : 0;
  kinfo.state = p->state;
  kinfo.sz = p->sz;
  safestrcpy(kinfo.name, p->name, sizeof(kinfo.name));

  if(copyout(p->pagetable, (uint64)info, (char *)&kinfo, sizeof(kinfo)) < 0)
    return -1;

  return 0;
}
