#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int pid;
    
    printf("Priority Test Start\n");
    
    // Create child process with high priority
    pid = fork();
    if(pid == 0) {
        // Child process
        setpriority(getpid(), 0);  // highest priority
        printf("Child (PID: %d, Priority: 0) running\n", getpid());
        
        // Do some work
        for(int i = 0; i < 3; i++) {
            printf("Child: work %d\n", i+1);
            sleep(1);
        }
        
        printf("Child finished\n");
        exit(0);
    }
    
    // Parent process with lower priority
    setpriority(getpid(), 20);  // low priority
    printf("Parent (PID: %d, Priority: 20) running\n", getpid());
    
    // Do some work
    for(int i = 0; i < 3; i++) {
        printf("Parent: work %d\n", i+1);
        sleep(1);
    }
    
    // Wait for child
    wait(0);
    
    printf("Priority Test End\n");
    exit(0);
}
