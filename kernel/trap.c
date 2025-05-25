#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

// Initialize trap handling.
// Called once during kernel initialization.
void
trapinit(void)
{
  // Initialize the lock that protects the timer tick counter
  initlock(&tickslock, "time");
}

// Set up CPU-specific trap handling.
// Called for each CPU during initialization.
// Configures the CPU to use kernelvec as the trap handler
// when in kernel mode.
void
trapinithart(void)
{
  // Set the trap vector (handler) address for supervisor mode
  // This tells the CPU where to jump when a trap occurs in kernel mode
  w_stvec((uint64)kernelvec);
}

//
// Handle an interrupt, exception, or system call from user space.
// Called from trampoline.S when a trap occurs while in user mode.
//
void
usertrap(void)
{
  int which_dev = 0;

  // Verify we came from user mode, not kernel mode
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // Configure trap handling for kernel mode
  // Now that we're in the kernel, future traps should go to kerneltrap()
  w_stvec((uint64)kernelvec);

  // Get the current process
  struct proc *p = myproc();
  
  // Save the user program counter where execution will resume later
  p->trapframe->epc = r_sepc();
  
  // Check the cause of the trap
  if(r_scause() == 8){
    // System call (ecall instruction)

    // If process is marked for killing, exit now
    if(killed(p))
      exit(-1);

    // Advance program counter past the ecall instruction (4 bytes)
    // so that we return to the next instruction after the system call
    p->trapframe->epc += 4;

    // Enable interrupts while handling system call
    // This is safe now that we've saved the important registers
    intr_on();

    // Handle the system call
    syscall();
  } else if((which_dev = devintr()) != 0){
    // Device interrupt was handled by devintr()
    // which_dev indicates which device caused the interrupt
  } else {
    // Unknown trap cause - likely a fault (page fault, illegal instruction, etc.)
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    // Mark the process for termination
    setkilled(p);
  }

  // Check again if process should be killed
  if(killed(p))
    exit(-1);

  // If this was a timer interrupt, yield the CPU to another process
  if(which_dev == 2)
    yield();

  // Return to user space
  usertrapret();
}

//
// Return to user space after handling a trap.
// This function prepares for and executes the transition back to user mode.
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // Disable interrupts during the transition
  // This prevents races while we're setting up the return path
  intr_off();

  // Configure trap handling for user mode
  // Set trap vector to point to uservec in trampoline.S
  // This is where the CPU will jump when a trap occurs in user mode
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // Set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  // These values allow the trampoline code to transition to kernel mode.
  p->trapframe->kernel_satp = r_satp();         // Kernel page table address
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // Kernel stack pointer
  p->trapframe->kernel_trap = (uint64)usertrap; // Address of trap handler
  p->trapframe->kernel_hartid = r_tp();         // CPU ID for cpuid()

  // Set up the registers for returning to user mode
  
  // Set S Previous Privilege mode to User in sstatus register
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // Clear SPP bit (set to 0 for user mode)
  x |= SSTATUS_SPIE; // Set SPIE bit to enable interrupts in user mode
  w_sstatus(x);

  // Set S Exception Program Counter to the saved user program counter
  // This is where execution will resume in user mode
  w_sepc(p->trapframe->epc);

  // Prepare the user page table address for trampoline.S
  uint64 satp = MAKE_SATP(p->pagetable);

  // Jump to userret in trampoline.S, which will:
  // 1. Switch to the user page table
  // 2. Restore user registers from trapframe
  // 3. Return to user mode with sret instruction
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Handle interrupts and exceptions that occur while in kernel mode.
// Called via kernelvec from assembly code in kernelvec.S.
// Runs on the current kernel stack of the current process or scheduler.
void
kerneltrap()
{
  int which_dev = 0;
  // Save important registers that might be modified
  uint64 sepc = r_sepc();     // Exception program counter
  uint64 sstatus = r_sstatus(); // Status register
  uint64 scause = r_scause();  // Cause of the trap
  
  // Verify we came from kernel mode (supervisor mode)
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  
  // Verify interrupts were disabled (they should be in kernel mode)
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  // Try to handle device interrupt
  if((which_dev = devintr()) == 0){
    // Not a device interrupt - must be an exception in kernel code
    // This is a serious error - kernel code shouldn't fault
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // For timer interrupts, yield the CPU to another process
  // But only if we're running a process (not the scheduler)
  if(which_dev == 2 && myproc() != 0)
    yield();

  // Restore saved registers before returning
  // This is necessary because yield() might have caused traps
  // that modified these registers
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// Handle timer (clock) interrupts.
// Called from devintr() when a timer interrupt occurs.
// Increments the system tick counter and wakes processes
// that are sleeping on the tick counter.
void
clockintr()
{
  // Only CPU 0 updates the tick counter to avoid races
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;                // Increment global tick counter
    wakeup(&ticks);         // Wake any processes sleeping on &ticks
    release(&tickslock);
  }

  // Schedule the next timer interrupt
  // This also acknowledges the current interrupt
  // 1000000 cycles is approximately 0.1 seconds on typical RISC-V hardware
  w_stimecmp(r_time() + 1000000);
}

// Check if the trap is a device interrupt and handle it.
// Called from both usertrap() and kerneltrap().
// Returns:
//   2 if it's a timer interrupt
//   1 if it's another device interrupt
//   0 if it's not a device interrupt (or not recognized)
int
devintr()
{
  // Read the cause of the trap
  uint64 scause = r_scause();

  // Check for external interrupt (bit 63 set, code 9)
  if(scause == 0x8000000000000009L){
    // This is a supervisor external interrupt from a device via PLIC
    // (Platform-Level Interrupt Controller)

    // Claim the interrupt - this tells the PLIC we're handling it
    // and returns which device caused the interrupt
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      // UART (serial port) interrupt
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      // Disk interrupt
      virtio_disk_intr();
    } else if(irq){
      // Unknown device interrupt
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

