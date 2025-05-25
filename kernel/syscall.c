#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Safely fetch a 64-bit value from user memory at the given address.
// This is used to retrieve system call arguments that are pointers.
// Returns 0 on success, -1 on error (invalid address).
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  // Check if address is within process memory bounds
  // Both checks are needed to handle potential integer overflow
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  // Copy data from user space to kernel space
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Safely fetch a null-terminated string from user memory.
// Used to retrieve string arguments for system calls.
// Returns length of string (excluding null terminator) or -1 on error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  // Copy string from user space to kernel buffer
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

// Get the raw value of the nth system call argument.
// System call arguments are passed in registers a0-a5,
// which are saved in the process's trapframe during the trap.
static uint64
argraw(int n)
{
  struct proc *p = myproc();
  // Return the appropriate register value based on argument position
  switch (n) {
  case 0:
    return p->trapframe->a0;  // First argument
  case 1:
    return p->trapframe->a1;  // Second argument
  case 2:
    return p->trapframe->a2;  // Third argument
  case 3:
    return p->trapframe->a3;  // Fourth argument
  case 4:
    return p->trapframe->a4;  // Fifth argument
  case 5:
    return p->trapframe->a5;  // Sixth argument
  }
  panic("argraw");  // Should never happen - invalid argument number
  return -1;
}

// Fetch the nth system call argument as a 32-bit integer.
// Used for system calls that take integer arguments.
void
argint(int n, int *ip)
{
  *ip = argraw(n);  // Simply get the raw value
}

// Fetch the nth system call argument as a pointer (address).
// Used for system calls that take pointer arguments.
// Doesn't check if the pointer is valid - that's done later
// when the pointer is actually used with copyin/copyout.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);  // Simply get the raw value
}

// Fetch the nth system call argument as a null-terminated string.
// Used for system calls that take string arguments (like exec).
// Copies the string from user space to the provided kernel buffer.
// Returns string length (including null) if successful, -1 on error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  // First get the address where the string is stored
  argaddr(n, &addr);
  // Then fetch the string from that address
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

// Main system call dispatcher function.
// Called from usertrap() when a system call trap occurs.
// Determines which system call to execute based on the syscall number,
// executes it, and stores the return value.
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  // System call number is passed in the a7 register
  num = p->trapframe->a7;
  
  // Check if the system call number is valid and has an implementation
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Call the appropriate system call function
    // Store its return value in a0, which becomes the return value
    // seen by the user program
    p->trapframe->a0 = syscalls[num]();
  } else {
    // Invalid system call number
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;  // Return error to the user program
  }
}
