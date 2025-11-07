#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int pid1, pid2, pid3;
    
    printf("=== Priority Scheduling Demo ===\n");
    
    // Create first child process - high priority
    pid1 = fork();
    if(pid1 == 0) {
        setpriority(getpid(), 0);  // highest priority
        printf("Process1 (PID: %d, Priority: 0) starts execution\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("Process1: step %d\n", i+1);
            sleep(1);
        }
        printf("Process1 execution completed\n");
        exit(0);
    }
    
    // Create second child process - medium priority
    pid2 = fork();
    if(pid2 == 0) {
        setpriority(getpid(), 10); // medium priority
        printf("Process2 (PID: %d, Priority: 10) starts execution\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("Process2: step %d\n", i+1);
            sleep(1);
        }
        printf("Process2 execution completed\n");
        exit(0);
    }
    
    // Create third child process - low priority
    pid3 = fork();
    if(pid3 == 0) {
        setpriority(getpid(), 20); // low priority
        printf("Process3 (PID: %d, Priority: 20) starts execution\n", getpid());
        for(int i = 0; i < 5; i++) {
            printf("Process3: step %d\n", i+1);
            sleep(1);
        }
        printf("Process3 execution completed\n");
        exit(0);
    }
    
    // Parent process waits for all children
    wait(0);
    wait(0);
    wait(0);
    
    printf("=== Priority Scheduling Demo Completed ===\n");
    exit(0);
}
