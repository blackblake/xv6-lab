#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXP 32
#define QSIZE 64

typedef struct {
  int pid;
  int arrival;
  int burst;
  int type; // 0=SYSTEM,1=INTERACTIVE,2=BATCH
  // runtime fields
  int remaining;
  int start;      // first time scheduled
  int finish;     // completion time
  int responded;  // boolean
  int last_enq;   // for debugging
} Process;

static void reset_runtime(Process *procs, int n) {
  for(int i=0;i<n;i++){ procs[i].remaining=procs[i].burst; procs[i].start=-1; procs[i].finish=-1; procs[i].responded=0; }
}

static int min(int a,int b){return a<b?a:b;}

// metrics (avoid float in xv6: use fixed-point times 100)
static int average_waiting_time_x100(Process *procs,int n){
  int sum=0; for(int i=0;i<n;i++){ int turnaround = procs[i].finish - procs[i].arrival; int waiting = turnaround - procs[i].burst; if(waiting<0) waiting=0; sum+=waiting; }
  return n? (sum*100) / n : 0;
}
static int average_turnaround_time_x100(Process *procs,int n){ int sum=0; for(int i=0;i<n;i++){ sum += (procs[i].finish - procs[i].arrival); } return n? (sum*100)/n : 0; }
static int average_response_time_x100(Process *procs,int n){ int sum=0; for(int i=0;i<n;i++){ int first = procs[i].start; sum += (first - procs[i].arrival); } return n? (sum*100)/n : 0; }

static void print_avg_line(const char* prefix, int n, int aw100, int at100, int ar100){
  printf("%s: N=%d  Aw=%d.%02d  At=%d.%02d  Ar=%d.%02d\n", prefix, n,
         aw100/100, aw100%100, at100/100, at100%100, ar100/100, ar100%100);
}

// Simple sort by arrival (stable bubble sort for small n)
static void sort_by_arrival(Process *a,int n){
  for(int i=0;i<n;i++){
    for(int j=0;j+1<n-i;j++){
      if(a[j].arrival > a[j+1].arrival){ Process t=a[j]; a[j]=a[j+1]; a[j+1]=t; }
    }
  }
}

// FCFS
static void simulate_fcfs(Process *src,int n){
  Process p[MAXP]; for(int i=0;i<n;i++) p[i]=src[i]; reset_runtime(p,n); sort_by_arrival(p,n);
  int t=0; for(int i=0;i<n;i++){ if(t < p[i].arrival) t = p[i].arrival; p[i].start=t; t += p[i].burst; p[i].remaining=0; p[i].finish=t; }
  print_avg_line("FCFS", n,
                 average_waiting_time_x100(p,n),
                 average_turnaround_time_x100(p,n),
                 average_response_time_x100(p,n));
}

// Round Robin
typedef struct { int idx[QSIZE]; int f,r; } Queue;
static void qinit(Queue* q){ q->f=q->r=0; }
static int qempty(Queue* q){ return q->f==q->r; }
static void qpush(Queue* q,int v){ int nr=(q->r+1)%QSIZE; if(nr==q->f) return; q->idx[q->r]=v; q->r=nr; }
static int qpop(Queue* q){ if(qempty(q)) return -1; int v=q->idx[q->f]; q->f=(q->f+1)%QSIZE; return v; }

static void simulate_rr(Process *src,int n,int quantum){
  Process p[MAXP]; for(int i=0;i<n;i++) p[i]=src[i]; reset_runtime(p,n); sort_by_arrival(p,n);
  Queue q; qinit(&q);
  int t=0, completed=0, next=0;
  while(completed<n){
    // enqueue arrivals up to time t
    while(next<n && p[next].arrival<=t){ qpush(&q,next); next++; }
    if(qempty(&q)) { t = p[next].arrival; continue; }
    int i = qpop(&q);
    if(p[i].start==-1) p[i].start=t;
    int run = min(quantum, p[i].remaining);
    t += run;
    p[i].remaining -= run;
    // add arrivals during the run
    while(next<n && p[next].arrival<=t){ qpush(&q,next); next++; }
    if(p[i].remaining==0){ p[i].finish=t; completed++; }
    else { qpush(&q,i); }
  }
  char buf[16];
  // build prefix like "RR(q=3)"
  // simple integer to string
  int qval=quantum; int idx=0; buf[idx++]='R'; buf[idx++]='R'; buf[idx++]='('; buf[idx++]='q'; buf[idx++]='=';
  if(qval>=10){ buf[idx++]= '0'+(qval/10)%10; }
  buf[idx++]= '0'+(qval%10);
  buf[idx++]=')'; buf[idx]=0;
  print_avg_line(buf, n,
                 average_waiting_time_x100(p,n),
                 average_turnaround_time_x100(p,n),
                 average_response_time_x100(p,n));
}

// Multilevel Queue (3 levels) with fixed quantums 2/4/8 and priority
static void simulate_mlq(Process *src,int n){
  Process p[MAXP]; for(int i=0;i<n;i++) p[i]=src[i]; reset_runtime(p,n); sort_by_arrival(p,n);
  Queue q0,q1,q2; qinit(&q0); qinit(&q1); qinit(&q2);
  int quantum[3] = {2,4,8};
  int t=0, completed=0, next=0;
  while(completed<n){
    while(next<n && p[next].arrival<=t){ if(p[next].type==0) qpush(&q0,next); else if(p[next].type==1) qpush(&q1,next); else qpush(&q2,next); next++; }
    Queue* active=0; int level=-1;
    if(!qempty(&q0)){ active=&q0; level=0; }
    else if(!qempty(&q1)){ active=&q1; level=1; }
    else if(!qempty(&q2)){ active=&q2; level=2; }
    if(!active){ t = p[next].arrival; continue; }
    int i = qpop(active);
    if(p[i].start==-1) p[i].start=t;
    int run = min(quantum[level], p[i].remaining);
    t += run;
    p[i].remaining -= run;
    while(next<n && p[next].arrival<=t){ if(p[next].type==0) qpush(&q0,next); else if(p[next].type==1) qpush(&q1,next); else qpush(&q2,next); next++; }
    if(p[i].remaining==0){ p[i].finish=t; completed++; }
    else { // requeue at same level
      if(level==0) qpush(&q0,i); else if(level==1) qpush(&q1,i); else qpush(&q2,i);
    }
  }
  print_avg_line("MLQ(2/4/8)", n,
                 average_waiting_time_x100(p,n),
                 average_turnaround_time_x100(p,n),
                 average_response_time_x100(p,n));
}

static void run_suite(const char* title, Process* arr, int n){
  printf("\n=== %s ===\n", title);
  simulate_fcfs(arr,n);
  simulate_rr(arr,n,3);
  simulate_mlq(arr,n);
}

int main(int argc, char** argv){
  // CPU密集型5个进程，突发时间10-50，全部到达0
  Process cpu5[] = {
    {1,0,24,2,0,-1,-1,0,0},
    {2,0,3, 2,0,-1,-1,0,0},
    {3,0,3, 1,0,-1,-1,0,0},
    {4,0,20,0,0,-1,-1,0,0},
    {5,0,15,2,0,-1,-1,0,0},
  };

  // 不同到达时间
  Process stagger[] = {
    {1,0,10,2,0,-1,-1,0,0},
    {2,2,6, 1,0,-1,-1,0,0},
    {3,4,8, 2,0,-1,-1,0,0},
    {4,6,12,0,0,-1,-1,0,0},
  };

  // 混合负载
  Process mix[] = {
    {1,0,5, 0,0,-1,-1,0,0},
    {2,1,18,2,0,-1,-1,0,0},
    {3,2,6, 1,0,-1,-1,0,0},
    {4,3,10,2,0,-1,-1,0,0},
    {5,4,2, 0,0,-1,-1,0,0},
  };

  run_suite("CPU密集(5)", cpu5, sizeof(cpu5)/sizeof(cpu5[0]));
  run_suite("不同到达时间", stagger, sizeof(stagger)/sizeof(stagger[0]));
  run_suite("混合负载", mix, sizeof(mix)/sizeof(mix[0]));

  exit(0);
}
