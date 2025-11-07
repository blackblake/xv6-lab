#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 模拟进程结构（简化版）
typedef struct {
    int pid;
    int burst_time;
    int arrival_time;
    int state; // 0=READY, 1=RUNNING, 2=COMPLETED
    int proc_type; // 0=SYSTEM, 1=INTERACTIVE, 2=BATCH
    int queue_level; // 0=high, 1=medium, 2=low
    char name[16];
} Process;

// 队列容量（用户态测试用，避免栈溢出）
#define QSIZE 16

// 轮转队列结构
typedef struct {
    Process* queue[QSIZE];
    int front, rear;
    int time_quantum;
} RoundRobinQueue;

// 多级队列结构
typedef struct {
    RoundRobinQueue high_priority;   // 系统进程队列
    RoundRobinQueue medium_priority; // 交互进程队列
    RoundRobinQueue low_priority;    // 批处理进程队列
} MultiLevelQueue;

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
    if((rq->rear + 1) % QSIZE == rq->front) {
        return; // 队列满
    }
    rq->queue[rq->rear] = p;
    rq->rear = (rq->rear + 1) % QSIZE;
}

Process* dequeue(RoundRobinQueue* rq) {
    if(is_empty(rq)) {
        return 0;
    }
    Process* p = rq->queue[rq->front];
    rq->front = (rq->front + 1) % QSIZE;
    return p;
}

// 多级队列操作函数
void init_multilevel_queue(MultiLevelQueue* mlq) {
    init_queue(&mlq->high_priority, 2);    // 系统进程：短时间片
    init_queue(&mlq->medium_priority, 4);  // 交互进程：中等时间片
    init_queue(&mlq->low_priority, 8);     // 批处理进程：长时间片
}

void enqueue_to_level(MultiLevelQueue* mlq, Process* p, int level) {
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
            enqueue(&mlq->low_priority, p);
            p->queue_level = 2;
            break;
    }
}

Process* dequeue_from_level(MultiLevelQueue* mlq, int level) {
    Process* p = 0;
    switch(level) {
        case 0:
            p = dequeue(&mlq->high_priority);
            break;
        case 1:
            p = dequeue(&mlq->medium_priority);
            break;
        case 2:
            p = dequeue(&mlq->low_priority);
            break;
    }
    return p;
}

Process* schedule_from_multilevel(MultiLevelQueue* mlq) {
    Process* p = 0;
    
    // 检查高优先级队列
    if(!is_empty(&mlq->high_priority)) {
        p = dequeue_from_level(mlq, 0);
        if(p) return p;
    }
    
    // 检查中优先级队列
    if(!is_empty(&mlq->medium_priority)) {
        p = dequeue_from_level(mlq, 1);
        if(p) return p;
    }
    
    // 检查低优先级队列
    if(!is_empty(&mlq->low_priority)) {
        p = dequeue_from_level(mlq, 2);
        if(p) return p;
    }
    
    return 0;
}

void classify_process(Process* p) {
    if(strcmp(p->name, "init") == 0 || strcmp(p->name, "sh") == 0) {
        p->proc_type = 0; // SYSTEM_PROCESS
    } else if(strcmp(p->name, "hello") == 0 || strcmp(p->name, "echo") == 0) {
        p->proc_type = 1; // INTERACTIVE_PROCESS
    } else {
        p->proc_type = 2; // BATCH_PROCESS
    }
}

void migrate_process(MultiLevelQueue* mlq, Process* p, int from_level, int to_level) {
    if(from_level == to_level) return;
    
    enqueue_to_level(mlq, p, to_level);
    printf("进程 %d 从队列 %d 迁移到队列 %d\n", p->pid, from_level, to_level);
}

// 多级队列调度算法
void multilevel_queue_schedule(Process processes[], int n) {
    static MultiLevelQueue mlq; // 放入BSS，避免栈溢出
    init_multilevel_queue(&mlq);
    
    // 添加所有进程到适当的队列（按已给定的 proc_type）
    for(int i = 0; i < n; i++) {
        if(processes[i].state == 0) { // READY
            switch(processes[i].proc_type) {
                case 0: // SYSTEM_PROCESS
                    enqueue_to_level(&mlq, &processes[i], 0);
                    break;
                case 1: // INTERACTIVE_PROCESS
                    enqueue_to_level(&mlq, &processes[i], 1);
                    break;
                case 2: // BATCH_PROCESS
                    enqueue_to_level(&mlq, &processes[i], 2);
                    break;
            }
        }
    }
    
    printf("多级队列调度算法测试\n");
    printf("====================\n");
    
    int time = 0;
    int completed = 0;
    
    while(completed < n) {
        Process* current = schedule_from_multilevel(&mlq);
        if(current == 0) break;
        
        int quantum = 0;
        switch(current->queue_level) {
            case 0: quantum = 2; break;  // High priority
            case 1: quantum = 4; break;  // Medium priority
            case 2: quantum = 8; break;  // Low priority
        }
        
        printf("时间 %d: 进程 %d (类型: %d, 队列: %d) 开始执行\n", 
               time, current->pid, current->proc_type, current->queue_level);
        
        // 执行时间片
        int execution_time = (current->burst_time < quantum) ? 
                            current->burst_time : quantum;
        current->burst_time -= execution_time;
        time += execution_time;
        
        printf("时间 %d: 进程 %d 执行了 %d 个时间单位\n", 
               time, current->pid, execution_time);
        
        if(current->burst_time <= 0) {
            // 进程完成
            current->state = 2; // COMPLETED
            completed++;
            printf("时间 %d: 进程 %d 完成\n", time, current->pid);
        } else {
            // 进程未完成，重新加入队列
            current->state = 0; // READY
            enqueue_to_level(&mlq, current, current->queue_level);
            printf("时间 %d: 进程 %d 重新加入队列 %d\n", 
                   time, current->pid, current->queue_level);
        }
    }
    
    printf("====================\n");
    printf("所有进程调度完成，总时间: %d\n", time);
}

int main(int argc, char *argv[]) {
    printf("多级队列调度算法测试\n");
    printf("==================\n\n");
    
    // 测试用例1：不同类型进程的队列分配
    printf("测试用例1：不同类型进程的队列分配\n");
    static Process processes1[5];
    processes1[0] = (Process){1,10,0,0,0,0,"init"};
    processes1[1] = (Process){2,8,0,0,1,0,"hello"};
    processes1[2] = (Process){3,15,0,0,2,0,"batch1"};
    processes1[3] = (Process){4,6,0,0,1,0,"echo"};
    processes1[4] = (Process){5,12,0,0,2,0,"batch2"};
    
    multilevel_queue_schedule(processes1, 5);
    printf("\n");
    
    // 测试用例2：验证队列间优先级正确性
    printf("测试用例2：验证队列间优先级正确性\n");
    static Process processes2[4];
    processes2[0] = (Process){1,3,0,0,2,0,"batch1"};
    processes2[1] = (Process){2,2,0,0,1,0,"hello"};
    processes2[2] = (Process){3,1,0,0,0,0,"init"};
    processes2[3] = (Process){4,4,0,0,2,0,"batch2"};
    
    multilevel_queue_schedule(processes2, 4);
    printf("\n");
    
    // 测试用例3：进程迁移机制
    printf("测试用例3：进程迁移机制测试\n");
    static Process processes3[3];
    processes3[0] = (Process){1,20,0,0,2,0,"batch1"};
    processes3[1] = (Process){2,5,0,0,1,0,"hello"};
    processes3[2] = (Process){3,8,0,0,2,0,"batch2"};
    
    // 模拟进程迁移
    static MultiLevelQueue mlq; // 放入BSS
    init_multilevel_queue(&mlq);
    
    // 添加进程到队列
    for(int i = 0; i < 3; i++) {
        classify_process(&processes3[i]);
        enqueue_to_level(&mlq, &processes3[i], processes3[i].proc_type);
    }
    
    printf("初始队列分配完成\n");
    
    // 模拟进程迁移
    Process* p = dequeue_from_level(&mlq, 2); // 从低优先级队列取出
    if(p) {
        migrate_process(&mlq, p, 2, 1); // 迁移到中优先级队列
    }
    
    printf("\n测试完成！\n");
    exit(0);
}
