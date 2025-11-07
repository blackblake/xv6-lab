#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 测试优先级调度算法
int main(int argc, char *argv[])
{
    int pid;
    int i;
    
    printf("=== 优先级调度算法测试 ===\n");
    
    // 创建多个子进程，设置不同的优先级
    for(i = 0; i < 5; i++) {
        pid = fork();
        if(pid == 0) {
            // 子进程
            int priority = i * 5; // 优先级: 0, 5, 10, 15, 20
            int result = setpriority(getpid(), priority);
            if(result < 0) {
                printf("进程 %d: 设置优先级 %d 失败\n", getpid(), priority);
                exit(1);
            }
            
            printf("进程 %d: 优先级设置为 %d，开始执行\n", getpid(), priority);
            
            // 模拟CPU密集型工作
            for(int j = 0; j < 1000000; j++) {
                if(j % 100000 == 0) {
                    printf("进程 %d (优先级 %d): 执行进度 %d%%\n", 
                           getpid(), priority, (j * 100) / 1000000);
                }
            }
            
            printf("进程 %d (优先级 %d): 执行完成\n", getpid(), priority);
            exit(0);
        } else if(pid < 0) {
            printf("创建子进程失败\n");
            exit(1);
        }
    }
    
    // 父进程等待所有子进程完成
    for(i = 0; i < 5; i++) {
        wait(0);
    }
    
    printf("=== 优先级调度测试完成 ===\n");
    exit(0);
}
