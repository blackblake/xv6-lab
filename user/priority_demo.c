#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int pid1, pid2, pid3;
    
    printf("=== 优先级调度演示 ===\n");
    
    // 创建第一个子进程 - 高优先级
    pid1 = fork();
    if(pid1 == 0) {
        setpriority(getpid(), 0);  // 最高优先级
        printf("进程1 (PID: %d, 优先级: 0) 开始执行\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("进程1: 执行步骤 %d\n", i+1);
            sleep(1);
        }
        printf("进程1 执行完成\n");
        exit(0);
    }
    
    // 创建第二个子进程 - 中等优先级
    pid2 = fork();
    if(pid2 == 0) {
        setpriority(getpid(), 10); // 中等优先级
        printf("进程2 (PID: %d, 优先级: 10) 开始执行\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("进程2: 执行步骤 %d\n", i+1);
            sleep(1);
        }
        printf("进程2 执行完成\n");
        exit(0);
    }
    
    // 创建第三个子进程 - 低优先级
    pid3 = fork();
    if(pid3 == 0) {
        setpriority(getpid(), 20); // 低优先级
        printf("进程3 (PID: %d, 优先级: 20) 开始执行\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("进程3: 执行步骤 %d\n", i+1);
            sleep(1);
        }
        printf("进程3 执行完成\n");
        exit(0);
    }
    
    // 父进程等待所有子进程
    wait(0);
    wait(0);
    wait(0);
    
    printf("=== 优先级调度演示完成 ===\n");
    exit(0);
}
