#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

static uint32 urandom_seed = 1;
static struct spinlock urandom_lock;

static uint64 nullstat_count = 0;
static struct spinlock nullstat_lock;

int devnull_read(int minor, uint64 addr, int n) {
  struct proc *p = myproc();
  if (p == 0) return -1;
  switch (minor) {
  case 0: 
    return 0;
    
  case 1: { // zero
    char *buf = kalloc();
    memset(buf, 0, n);
    if (copyout(p->pagetable, addr, buf, n) < 0) {
      kfree(buf);
      return -1;
    }
    kfree(buf);
    return n;
  }
  
  case 2: { // urandom
    acquire(&urandom_lock);
    char *buf = kalloc();
    for (int i = 0; i < n; i++) {
      urandom_seed = urandom_seed * 1103515245 + 12345;
      buf[i] = (urandom_seed >> 16) & 0xFF;
    }
    if (copyout(p->pagetable, addr, buf, n) < 0) {
      kfree(buf);
      release(&urandom_lock);
      return -1;
    }
    kfree(buf);
    release(&urandom_lock);
    return n;
  }
  
  case 3: { // nullstat
    if (n != sizeof(uint64)) return -1;
    acquire(&nullstat_lock);
    uint64 count = nullstat_count;
    release(&nullstat_lock);
    if (copyout(p->pagetable, addr, (char*)&count, sizeof(count)) < 0)
      return -1;
    return sizeof(count);
  }
  
  default: return -1;
  }
}

int devnull_write(int minor, uint64 addr, int n) {
  struct proc *p = myproc();
  if (p == 0) return -1;
  switch (minor) {
  case 0: 
    return n;
    
  case 1: // zero
    return -1;
    
  case 2: { // urandom
    if (n != sizeof(urandom_seed)) return -1;
    uint32 seed;
    if (copyin(p->pagetable, (char*)&seed, addr, n) < 0)
      return -1;
    acquire(&urandom_lock);
    urandom_seed = seed;
    release(&urandom_lock);
    return n;
  }
  
  case 3: { // nullstat
    acquire(&nullstat_lock);
    nullstat_count += n;
    release(&nullstat_lock);
    return n;
  }
  
  default: return -1;
  }
}

void nulldevinit() {
  initlock(&urandom_lock, "urandom");
  initlock(&nullstat_lock, "nullstat");
}