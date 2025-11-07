#include "kernel/types.h"
#include "user/user.h"

static void test_getprocinfo(void) {
  struct procinfo info;
  printf("Testing getprocinfo()...\n");
  if (getprocinfo(&info) == 0) {
    printf("PID: %d, PPID: %d, State: %d\n", info.pid, info.ppid, info.state);
    printf("Size: %d, Name: %s\n", info.sz, info.name);
    printf("getprocinfo() test PASSED\n");
  } else {
    printf("getprocinfo() test FAILED\n");
  }
}

static void test_getsystime(void) {
  struct systime t;
  printf("Testing getsystime()...\n");
  if (getsystime(&t) == 0) {
    printf("Ticks: %d, Uptime: %d seconds\n", t.ticks, t.uptime);
    printf("getsystime() test PASSED\n");
  } else {
    printf("getsystime() test FAILED\n");
  }
}

static void test_setpriority(void) {
  int pid = getpid();
  printf("Testing setpriority()...\n");
  if (setpriority(pid, 5) == 0) {
    printf("Set priority to 5: PASSED\n");
  } else {
    printf("Set priority to 5: FAILED\n");
  }

  if (setpriority(pid, -1) == -1) {
    printf("Invalid priority test: PASSED\n");
  } else {
    printf("Invalid priority test: FAILED\n");
  }

  if (setpriority(99999, 5) == -1) {
    printf("Invalid PID test: PASSED\n");
  } else {
    printf("Invalid PID test: FAILED\n");
  }
}

int
main(void)
{
  printf("Starting system call tests...\n");
  test_getprocinfo();
  test_getsystime();
  test_setpriority();
  printf("All tests completed!\n");
  exit(0);
}


