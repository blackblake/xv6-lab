#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 模拟进程结构（简化版）
typedef struct {
    int pid;
    int burst_time;
    int arrival_time;
    int state; // 0=READY, 1=RUNNING, 2=COMPLETED
} Process;

// 轮转队列结构
typedef struct {
    Process* queue[64];
    int front, rear;
    int time_quantum;
} RoundRobinQueue;

// 队列操作函数
void init_queue(RoundRobinQueue* rq, int quantum) {
    rq->front = 0;
    rq->rear = 0;
    rq->time_quantum = quantum;
}

int is_empty(RoundRobinQueue* rq) {
    return rq->front == rq->rear;
}

void enqueue(RoundRobinQueue* rq, Process* p) {
    if((rq->rear + 1) % 64 == rq->front) {
        return; // 队列满
    }
    rq->queue[rq->rear] = p;
    rq->rear = (rq->rear + 1) % 64;
}

Process* dequeue(RoundRobinQueue* rq) {
    if(is_empty(rq)) {
        return 0;
    }
    Process* p = rq->queue[rq->front];
    rq->front = (rq->front + 1) % 64;
    return p;
}

// 轮转调度算法
void round_robin_schedule(Process processes[], int n, int quantum) {
    RoundRobinQueue rq;
    init_queue(&rq, quantum);
    
    // 将所有就绪进程加入队列
    for(int i = 0; i < n; i++) {
        if(processes[i].state == 0) { // READY
            enqueue(&rq, &processes[i]);
        }
    }
    
    printf("时间片轮转调度算法 (时间片大小: %d)\n", quantum);
    printf("=====================================\n");
    
    int time = 0;
    int completed = 0;
    
    while(!is_empty(&rq) && completed < n) {
        Process* current = dequeue(&rq);
        if(current == 0) break;
        
        printf("时间 %d: 进程 %d 开始执行\n", time, current->pid);
        
        // 执行时间片
        int execution_time = (current->burst_time < quantum) ? current->burst_time : quantum;
        current->burst_time -= execution_time;
        time += execution_time;
        
        printf("时间 %d: 进程 %d 执行了 %d 个时间单位\n", time, current->pid, execution_time);
        
        if(current->burst_time <= 0) {
            // 进程完成
            current->state = 2; // COMPLETED
            completed++;
            printf("时间 %d: 进程 %d 完成\n", time, current->pid);
        } else {
            // 进程未完成，重新加入队列
            current->state = 0; // READY
            enqueue(&rq, current);
            printf("时间 %d: 进程 %d 重新加入队列，剩余时间: %d\n", time, current->pid, current->burst_time);
        }
    }
    
    printf("=====================================\n");
    printf("所有进程调度完成，总时间: %d\n", time);
}

int main(int argc, char *argv[]) {
    printf("时间片轮转调度算法测试\n");
    printf("====================\n\n");
    
    // 测试用例1：不同时间片大小
    printf("测试用例1：不同时间片大小\n");
    Process processes1[] = {
        {1, 10, 0, 0}, // PID=1, 突发时间=10, 到达时间=0, 状态=READY
        {2, 15, 0, 0}, // PID=2, 突发时间=15, 到达时间=0, 状态=READY
        {3, 8, 0, 0},  // PID=3, 突发时间=8, 到达时间=0, 状态=READY
        {4, 12, 0, 0}  // PID=4, 突发时间=12, 到达时间=0, 状态=READY
    };
    
    printf("时间片大小 = 3:\n");
    round_robin_schedule(processes1, 4, 3);
    printf("\n");
    
    // 重置进程状态
    for(int i = 0; i < 4; i++) {
        processes1[i].state = 0;
    }
    processes1[0].burst_time = 10;
    processes1[1].burst_time = 15;
    processes1[2].burst_time = 8;
    processes1[3].burst_time = 12;
    
    printf("时间片大小 = 5:\n");
    round_robin_schedule(processes1, 4, 5);
    printf("\n");
    
    // 测试用例2：验证轮转顺序
    printf("测试用例2：验证轮转顺序\n");
    Process processes2[] = {
        {1, 6, 0, 0},
        {2, 4, 0, 0},
        {3, 2, 0, 0}
    };
    
    printf("时间片大小 = 2:\n");
    round_robin_schedule(processes2, 3, 2);
    printf("\n");
    
    // 测试用例3：时间片用尽的处理
    printf("测试用例3：时间片用尽的处理\n");
    Process processes3[] = {
        {1, 1, 0, 0},  // 短进程
        {2, 10, 0, 0}, // 长进程
        {3, 2, 0, 0}   // 中等进程
    };
    
    printf("时间片大小 = 3:\n");
    round_robin_schedule(processes3, 3, 3);
    
    printf("\n测试完成！\n");
    exit(0);
}
