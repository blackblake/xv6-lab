#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#ifdef USE_COLOR
#define FG_GREEN "\x1b[1;32m"
#define FG_RESET "\x1b[0m"
#else
#define FG_GREEN ""
#define FG_RESET ""
#endif

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    if (cpuid() == 0) {
      printf("\x1b[1;32m\n");
      printf("=============================================\n");
      printf("   Welcome to xv6-riscv (Customized Boot)   \n");
      printf("=============================================\n\x1b[0m");
    }
    extern char end[];        // 链接脚本导出的内核末尾
    uint64 total = PHYSTOP - KERNBASE;
    uint64 used  = (uint64)end - KERNBASE;
    uint64 free  = total - used;

  
  int total_kb = (int)(total/1024);
  int used_kb  = (int)(used/1024);
  int free_kb  = (int)(free/1024);
  printf("Memory: total=%d KB, used=%d KB, free=%d KB\n", total_kb, used_kb, free_kb);
    printf("CPU: cpuid=%d, NCPU(max)=%d\n", cpuid(), NCPU);

    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
