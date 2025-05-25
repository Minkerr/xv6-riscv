// Saved registers for kernel context switches.
// When switching from one process to another, these registers
// need to be saved and restored to resume execution correctly.
struct context {
  uint64 ra;   // Return address (ra) register
  uint64 sp;   // Stack pointer (sp) register

  // RISC-V callee-saved registers that must be preserved across function calls
  uint64 s0;   // Also frame pointer (fp)
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state - each CPU has its own instance of this structure
struct cpu {
  struct proc *proc;          // The process currently running on this CPU, or null if idle
  struct context context;     // When switching to scheduler(), save context here
  int noff;                   // Depth of push_off() nesting (for interrupt disabling)
  int intena;                 // Were interrupts enabled before push_off()?
};

// Array of CPU structures, one per CPU in the system
extern struct cpu cpus[NCPU];

// Trapframe: per-process data for trap handling code in trampoline.S.
// This structure facilitates the transition between user and kernel mode.
// It's located in a dedicated page just below the trampoline page in the
// user page table, but not mapped in the kernel page table.
//
// When a trap occurs:
// 1. uservec in trampoline.S saves all user registers in this trapframe
// 2. It then loads kernel_sp, kernel_hartid, kernel_satp from the trapframe
// 3. It switches to the kernel page table and jumps to usertrap()
//
// When returning to user space:
// 1. usertrapret() and userret in trampoline.S set up kernel_* fields
// 2. They restore user registers from the trapframe
// 3. They switch to the user page table and return to user space
//
// The trapframe includes all callee-saved registers because the return path
// to user space doesn't go through the normal kernel call stack unwinding.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

// Process states in the xv6 operating system
enum procstate {
  UNUSED,    // Process slot is free
  USED,      // Process is allocated but not ready
  SLEEPING,  // Process is sleeping (waiting for an event)
  RUNNABLE,  // Process is ready to run
  RUNNING,   // Process is currently running on a CPU
  ZOMBIE     // Process has terminated but not yet freed
};

// Per-process state - the core structure representing a process in xv6
struct proc {
  struct spinlock lock;        // Protects most fields in this structure

  // Fields that require p->lock to be held when accessing:
  enum procstate state;        // Current process state
  void *chan;                  // If sleeping, the channel (event) it's waiting on
  int killed;                  // Set to non-zero if process should be killed
  int xstate;                  // Exit status code to be returned to parent's wait()
  int pid;                     // Process identifier

  // Fields that require wait_lock to be held:
  struct proc *parent;         // Parent process (for wait() and inheritance)

  // Fields that are private to the process (no lock needed):
  uint64 kstack;               // Virtual address of this process's kernel stack
  uint64 sz;                   // Size of process memory in bytes
  pagetable_t pagetable;       // Page table for this process's address space
  struct trapframe *trapframe; // Saved user registers during system calls/interrupts
  struct context context;      // Saved kernel context for context switching
  struct file *ofile[NOFILE];  // Open file descriptors
  struct inode *cwd;           // Current working directory
  char name[16];               // Process name (for debugging)
};
