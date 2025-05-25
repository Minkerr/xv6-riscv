#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

// System call implementation for exit()
// Terminates the current process with the specified exit status.
// The exit status is passed to the parent in wait().
uint64
sys_exit(void)
{
  int n;
  // Get the exit status from user space (first argument)
  argint(0, &n);
  // Terminate the process with this status
  exit(n);
  return 0;  // This line is never reached (exit doesn't return)
}

// System call implementation for getpid()
// Returns the process ID of the calling process.
uint64
sys_getpid(void)
{
  // Simply return the pid field of the current process
  return myproc()->pid;
}

// System call implementation for fork()
// Creates a new child process that is a copy of the calling process.
// Returns child's PID to the parent, 0 to the child.
uint64
sys_fork(void)
{
  // Call the kernel's fork implementation
  return fork();
}

// System call implementation for wait()
// Waits for a child process to exit and retrieves its exit status.
// Returns the PID of the terminated child, or -1 if no children.
uint64
sys_wait(void)
{
  uint64 p;
  // Get pointer where to store the exit status
  argaddr(0, &p);
  // Wait for any child to exit and return its PID
  return wait(p);
}

// System call implementation for sbrk()
// Grows or shrinks the process's memory size by n bytes.
// Returns the old size before the change.
uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  // Get the requested change in size (can be positive or negative)
  argint(0, &n);
  // Save the current size before changing it
  addr = myproc()->sz;
  // Attempt to grow or shrink the process memory
  if(growproc(n) < 0)
    return -1;  // Failed to change size
  return addr;  // Return the old size
}

// System call implementation for sleep()
// Puts the calling process to sleep for n clock ticks.
// A tick is a system-defined time unit (~0.1 second).
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  // Get the number of ticks to sleep
  argint(0, &n);
  // Ensure non-negative sleep time
  if(n < 0)
    n = 0;
    
  // Acquire lock protecting the ticks counter
  acquire(&tickslock);
  // Record the starting tick count
  ticks0 = ticks;
  
  // Sleep until enough ticks have passed
  while(ticks - ticks0 < n){
    // If the process is killed while sleeping, exit early
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    // Put the process to sleep, waiting for ticks to change
    // This releases the tickslock while sleeping
    sleep(&ticks, &tickslock);
  }
  
  // Release the lock and return success
  release(&tickslock);
  return 0;
}

// System call implementation for kill()
// Marks a process with the specified PID for termination.
// The process will exit at a safe point (e.g., when returning to user space).
uint64
sys_kill(void)
{
  int pid;

  // Get the PID of the process to kill
  argint(0, &pid);
  // Mark the process for termination
  return kill(pid);
}

// System call implementation for uptime()
// Returns the number of clock ticks since system boot.
// Used to measure elapsed time.
uint64
sys_uptime(void)
{
  uint xticks;

  // Safely access the global tick counter
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
