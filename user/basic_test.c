#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    printf("Basic test start\n");
    
    // Test setpriority system call
    int result = setpriority(getpid(), 5);
    if(result == 0) {
        printf("setpriority success\n");
    } else {
        printf("setpriority failed\n");
    }
    
    printf("Basic test end\n");
    exit(0);
}
