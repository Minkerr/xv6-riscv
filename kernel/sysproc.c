#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void
print_pte(pagetable_t pagetable, int level, uint64 va)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V) {
      uint64 child_va = va | (((uint64)i) << (PGSHIFT + 9*level));
      
      for(int j = 0; j < level; j++) {
        printf("......... ");
      }
      
      printf("0x");
      printf("%x", i);
      printf(" -> 0x");
      printf("%lx", PTE2PA(pte));
      printf(" ");
      
      char r = (pte & PTE_R) ? 'R' : '_';
      char w = (pte & PTE_W) ? 'W' : '_';
      char x = (pte & PTE_X) ? 'X' : '_';
      char u = (pte & PTE_U) ? 'U' : '_';
      char g = (pte & PTE_G) ? 'G' : '_';
      char a = (pte & PTE_A) ? 'A' : '_';
      char d = (pte & PTE_D) ? 'D' : '_';
      
      consputc(r);
      consputc(w);
      consputc(x);
      consputc(u);
      consputc(g);
      consputc(a);
      consputc(d);
      consputc('\n');
      
      if(level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        uint64 child = PTE2PA(pte);
        print_pte((pagetable_t)child, level-1, child_va);
      }
    }
  }
}

int
is_addr_in_buffer(uint64 va, uint64 buf_start, uint64 buf_len)
{
  if (buf_start == 0 || buf_len == 0)
    return 1;
  
  uint64 buf_end = buf_start + buf_len;
  uint64 page_start = PGROUNDDOWN(va);
  uint64 page_end = page_start + PGSIZE;
  
  return (page_start < buf_end && buf_start < page_end);
}

void
process_pte(pagetable_t pagetable, int level, uint64 va, uint64 buf_start, uint64 buf_len, int flags_mask, int clear_flags)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V) {
      uint64 child_va = va | (((uint64)i) << (PGSHIFT + 9*level));
      
      if(level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        uint64 child = PTE2PA(pte);
        process_pte((pagetable_t)child, level-1, child_va, buf_start, buf_len, flags_mask, clear_flags);
      } else if (level == 0) {
        if (is_addr_in_buffer(child_va, buf_start, buf_len)) {
          if (clear_flags) {
            if (flags_mask & 1)
              pagetable[i] &= ~PTE_D;
            if (flags_mask & 2)
              pagetable[i] &= ~PTE_A;
          } else {
            if (flags_mask == 0 ||
                ((flags_mask & 1) && (pte & PTE_D)) ||
                ((flags_mask & 2) && (pte & PTE_A))) {
              
              printf("0x");
              printf("%x", i);
              printf(" -> 0x");
              printf("%lx", PTE2PA(pte));
              printf(" ");
              
              char r = (pte & PTE_R) ? 'R' : '_';
              char w = (pte & PTE_W) ? 'W' : '_';
              char x = (pte & PTE_X) ? 'X' : '_';
              char u = (pte & PTE_U) ? 'U' : '_';
              char g = (pte & PTE_G) ? 'G' : '_';
              char a = (pte & PTE_A) ? 'A' : '_';
              char d = (pte & PTE_D) ? 'D' : '_';
              
              consputc(r);
              consputc(w);
              consputc(x);
              consputc(u);
              consputc(g);
              consputc(a);
              consputc(d);
              consputc('\n');
            }
          }
        }
      }
    }
  }
}

uint64
sys_pageinfo(void)
{
  uint64 buf_addr;
  int buf_len;
  int flags_mask;
  
  argaddr(0, &buf_addr);
  argint(1, &buf_len);
  argint(2, &flags_mask);
  
  if (flags_mask < 0 || flags_mask > 3) {
    return -1;
  }
  
  struct proc *p = myproc();
  
  printf("PAGETABLE 0x");
  printf("%lx", (uint64)p->pagetable);
  printf("\n");
  
  if (flags_mask == 0) {
    print_pte(p->pagetable, 2, 0);
  } else {
    process_pte(p->pagetable, 2, 0, buf_addr, buf_len, flags_mask, 0);
  }
  
  return 0;
}

uint64
sys_clearflags(void)
{
  uint64 buf_addr;
  int buf_len;
  int flags_mask;
  
  argaddr(0, &buf_addr);
  argint(1, &buf_len);
  argint(2, &flags_mask);
  
  if (flags_mask < 0 || flags_mask > 3) {
    return -1;
  }
  
  struct proc *p = myproc();
  process_pte(p->pagetable, 2, 0, buf_addr, buf_len, flags_mask, 1);
  
  return 0;
}
