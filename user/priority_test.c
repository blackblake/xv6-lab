#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 简单的优先级调度测试
int main(int argc, char *argv[])
{
    int pid;
    int i;
    
    printf("优先级调度测试开始\n");
    
    // 创建3个子进程，设置不同优先级
    for(i = 0; i < 3; i++) {
        pid = fork();
        if(pid == 0) {
            // 子进程
            int priority = i * 10; // 优先级: 0, 10, 20
            setpriority(getpid(), priority);
            
            printf("子进程 %d: 优先级 %d，开始工作\n", getpid(), priority);
            
            // 简单的工作循环
            for(int j = 0; j < 10; j++) {
                printf("进程 %d (优先级 %d): 工作 %d/10\n", getpid(), priority, j+1);
                sleep(1); // 让出CPU
            }
            
            printf("子进程 %d: 工作完成\n", getpid());
            exit(0);
        }
    }
    
    // 父进程等待
    for(i = 0; i < 3; i++) {
        wait(0);
    }
    
    printf("优先级调度测试完成\n");
    exit(0);
}
