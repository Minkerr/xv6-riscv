#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// Flag to synchronize CPU initialization
volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
// This is the main entry point for the kernel
void
main()
{
  // CPU 0 initializes the system, other CPUs wait
  if(cpuid() == 0){
    consoleinit();    // Initialize console for output
    printfinit();     // Initialize printf functionality
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();          // Initialize physical memory allocator
    kvminit();        // Create kernel page table
    kvminithart();    // Load kernel page table into SATP register and enable paging
    procinit();       // Initialize process table and allocate kernel stacks
    trapinit();       // Initialize trap vectors and global trap state
    trapinithart();   // Set up CPU-specific trap handling
    plicinit();       // Initialize Platform-Level Interrupt Controller
    plicinithart();   // Configure PLIC for this CPU
    binit();          // Initialize buffer cache for file system
    iinit();          // Initialize inode cache for file system
    fileinit();       // Initialize file table
    virtio_disk_init(); // Initialize disk driver
    userinit();       // Create first user process (init)
    __sync_synchronize(); // Memory barrier to ensure all initialization is visible
    started = 1;      // Signal other CPUs to start
  } else {
    // Other CPUs wait until CPU 0 has finished initialization
    while(started == 0)
      ;
    __sync_synchronize(); // Memory barrier to ensure visibility of initialization
    printf("hart %d starting\n", cpuid());
    kvminithart();    // Enable paging on this CPU
    trapinithart();   // Set up trap handling on this CPU
    plicinithart();   // Configure interrupts for this CPU
  }

  // All CPUs enter the scheduler to start running processes
  scheduler();        // Never returns
}
