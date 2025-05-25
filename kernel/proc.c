#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page to catch stack overflows.
//
// Each process needs its own kernel stack when executing in kernel mode.
// These stacks are allocated during kernel initialization.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    // Allocate physical memory for the stack
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    // Calculate virtual address for this process's kernel stack
    // Each stack gets a unique virtual address
    uint64 va = KSTACK((int) (p - proc));
    // Map the virtual address to the physical memory in kernel page table
    // with read and write permissions
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// Initialize the process table and process locks.
// Called once during kernel initialization.
void
procinit(void)
{
  struct proc *p;
  
  // Initialize global locks
  initlock(&pid_lock, "nextpid");   // Protects the nextpid variable
  initlock(&wait_lock, "wait_lock"); // Protects parent-child relationships
  
  // Initialize each process structure
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");   // Per-process lock
      p->state = UNUSED;            // Mark all processes as free initially
      // Set kernel stack virtual address (physical memory allocated later)
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Return the ID of the current CPU.
// Must be called with interrupts disabled to avoid races,
// as the process might be moved to a different CPU.
int
cpuid()
{
  // In RISC-V, the tp register holds the CPU ID (hart ID)
  int id = r_tp();
  return id;
}

// Return a pointer to the current CPU's cpu structure.
// Interrupts must be disabled to prevent races.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return a pointer to the current process.
// Returns zero if no process is running on this CPU.
// This is a key function used throughout the kernel to
// access the current process's data.
struct proc*
myproc(void)
{
  // Disable interrupts to ensure atomic access
  push_off();
  // Get the current CPU
  struct cpu *c = mycpu();
  // Get the process running on this CPU
  struct proc *p = c->proc;
  // Re-enable interrupts
  pop_off();
  return p;
}

// Allocate a new process ID.
// This function ensures that each process gets a unique PID.
int
allocpid()
{
  int pid;
  
  // Acquire lock to ensure atomic access to nextpid
  acquire(&pid_lock);
  // Get the next available PID
  pid = nextpid;
  // Increment for the next process
  nextpid = nextpid + 1;
  // Release the lock
  release(&pid_lock);

  return pid;
}

// Allocate and initialize a new process structure.
// This is a key function used when creating new processes (fork, userinit).
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  // Search for an UNUSED process slot
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;  // No free process slots available

found:
  // Initialize process state
  p->pid = allocpid();  // Assign a new process ID
  p->state = USED;      // Mark as allocated but not runnable yet

  // Allocate memory for the trapframe
  // The trapframe stores user registers during system calls/interrupts
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Create an empty user page table for the process
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up kernel context for the new process
  // When this process is scheduled, it will start executing at forkret()
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;  // Return address points to forkret
  p->context.sp = p->kstack + PGSIZE;  // Stack pointer at top of kernel stack

  return p;  // Return with p->lock still held
}

// Free a process structure and all associated resources.
// Called when a process exits or is killed.
// p->lock must be held by the caller.
static void
freeproc(struct proc *p)
{
  // Free the trapframe memory
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  
  // Free the page table and all user memory
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  
  // Reset all process fields to default values
  p->sz = 0;            // Process memory size
  p->pid = 0;           // Process ID
  p->parent = 0;        // Parent process
  p->name[0] = 0;       // Process name
  p->chan = 0;          // Sleep channel
  p->killed = 0;        // Kill flag
  p->xstate = 0;        // Exit status
  p->state = UNUSED;    // Mark slot as available
}

// Create a user page table for a given process.
// This creates the initial page table with no user memory,
// but with the essential trampoline and trapframe pages that
// enable transitions between user and kernel mode.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // Create an empty page table structure
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // Map the trampoline code at the highest user virtual address.
  // The trampoline contains code for transitioning between user and kernel mode.
  // It's mapped without PTE_U because only the kernel needs to execute it.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // Map the trapframe page just below the trampoline page.
  // The trapframe stores user registers during system calls and interrupts.
  // It's accessible by both user and kernel code.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table and all associated physical memory.
// This is called when a process exits to clean up its resources.
// First unmaps special pages (trampoline and trapframe), then
// frees all user memory and the page table itself.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  // Unmap the trampoline page (without freeing physical memory since it's shared)
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  
  // Unmap the trapframe page (without freeing physical memory, handled by freeproc)
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  
  // Free all user memory pages and page table pages
  uvmfree(pagetable, sz);
}

// Binary code for the first user process (init)
// This is the compiled machine code from ../user/initcode.S
// It's a small program that executes the exec("/init") system call
// to load the real init program from the file system
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, // auipc a0, 0; addi a0, a0, 0x44
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02, // auipc a1, 0; addi a1, a1, 0x35
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00, // li a7, SYS_exec; ecall
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00, // li a7, SYS_exit; ecall
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69, // jal -4; "/ini"
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, // "t\0"; address of "/init\0"
  0x00, 0x00, 0x00, 0x00                          // NULL terminator for argv[]
};

// Set up the first user process (init).
// This is called once during kernel initialization.
// It creates the first process that will run the init program.
void
userinit(void)
{
  struct proc *p;

  // Allocate and initialize a process structure
  p = allocproc();
  // Save a pointer to this process as the init process
  // The init process is special - it becomes the parent of orphaned processes
  initproc = p;
  
  // Set up the initial program for the init process
  // This loads the initcode array into the process's address space
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;  // Process size is one page

  // Set up the initial user-mode registers
  p->trapframe->epc = 0;      // Start execution at virtual address 0
  p->trapframe->sp = PGSIZE;  // Stack starts at the top of the memory

  // Set process name and current working directory
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");  // Root directory

  // Mark the process as ready to run
  p->state = RUNNABLE;

  // Release the process lock so it can be scheduled
  release(&p->lock);
}

// Grow or shrink the process memory size.
// Used by the sbrk() system call and exec().
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  // Get current size
  sz = p->sz;
  
  if(n > 0){
    // Grow process memory
    // Allocate new memory and map it into the process's address space
    // PTE_W flag makes the memory writable
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      // Failed to allocate memory
      return -1;
    }
  } else if(n < 0){
    // Shrink process memory
    // Deallocate memory from the process's address space
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  
  // Update the process size
  p->sz = sz;
  return 0;
}

// Create a new process by duplicating the current one.
// This implements the fork() system call.
// Returns the child PID in the parent, 0 in the child, or -1 on error.
int
fork(void)
{
  int i, pid;
  struct proc *np;            // New process
  struct proc *p = myproc();  // Current (parent) process

  // Allocate and initialize a new process structure
  if((np = allocproc()) == 0){
    return -1;  // No free process slots or memory allocation failed
  }

  // Copy the parent's memory to the child
  // This creates an exact duplicate of the parent's address space
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;  // Memory copy failed
  }
  np->sz = p->sz;  // Set child size to match parent

  // Copy the parent's trapframe to the child
  // This includes all user registers
  *(np->trapframe) = *(p->trapframe);

  // Make fork() return 0 in the child process
  // In RISC-V, a0 register holds the return value
  np->trapframe->a0 = 0;

  // Copy open file descriptors from parent to child
  // This allows the child to access the same files
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);  // Increment reference count
      
  // Set child's current working directory
  np->cwd = idup(p->cwd);

  // Copy the parent's name to the child
  safestrcpy(np->name, p->name, sizeof(p->name));

  // Remember the child's PID for return value to parent
  pid = np->pid;

  // Release child's lock temporarily
  release(&np->lock);

  // Set up parent-child relationship
  acquire(&wait_lock);
  np->parent = p;  // Set child's parent pointer
  release(&wait_lock);

  // Mark child as ready to run
  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  // Return child's PID to the parent
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    intr_on();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
