#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "proc.h"

#define PSEUDO_MAJOR 2
#define PSEUDO_NULL 0
#define PSEUDO_ZERO 1
#define PSEUDO_URANDOM 2
#define PSEUDO_NULLSTAT 3

struct {
  struct spinlock lock;
  uint64 nullstat_count;
  uint seed;
} pseudo;

#define LCG_A 1664525
#define LCG_C 1013904223

static uint
lcg_random(uint seed)
{
  return seed * LCG_A + LCG_C;
}

int
pseudoread(int user_dst, short minor, uint64 dst, int n)
{
  int i;
  char c;
  uint64 size;
  
  switch(minor) {
    case PSEUDO_NULL:
      return 0;
    case PSEUDO_ZERO:
      c = 0;
      for(i = 0; i < n; i++) {
        if(either_copyout(user_dst, dst + i, &c, 1) == -1)
          break;
      }
      return i;
    case PSEUDO_URANDOM:
      acquire(&pseudo.lock);
      for(i = 0; i < n; i++) {
        pseudo.seed = lcg_random(pseudo.seed);
        c = (char)(pseudo.seed & 0xFF);
        if(either_copyout(user_dst, dst + i, &c, 1) == -1)
          break;
      }
      release(&pseudo.lock);
      return i;
      
    case PSEUDO_NULLSTAT:
      acquire(&pseudo.lock);
      if(n != sizeof(uint64)) {
        release(&pseudo.lock);
        return -1;
      }
      size = pseudo.nullstat_count;
      if(either_copyout(user_dst, dst, &size, sizeof(uint64)) == -1) {
        release(&pseudo.lock);
        return -1;
      }
      release(&pseudo.lock);
      return sizeof(uint64);
      
    default:
      return -1;
  }
}

int
pseudowrite(int user_src, short minor, uint64 src, int n)
{
  uint new_seed;
  
  switch(minor) {
    case PSEUDO_NULL:
      return n;
      
    case PSEUDO_ZERO:
      return -1;
      
    case PSEUDO_URANDOM:
      if(n != sizeof(uint)) {
        return -1;
      }
      if(either_copyin(&new_seed, user_src, src, sizeof(uint)) == -1) {
        return -1;
      }
      acquire(&pseudo.lock);
      pseudo.seed = new_seed;
      release(&pseudo.lock);
      return sizeof(uint);
      
    case PSEUDO_NULLSTAT:
      acquire(&pseudo.lock);
      pseudo.nullstat_count += n;
      release(&pseudo.lock);
      return n;
      
    default:
      return -1;
  }
}

void
pseudoinit(void)
{
  initlock(&pseudo.lock, "pseudo");
  pseudo.nullstat_count = 0;
  pseudo.seed = 0x12345678;
  
  devsw[PSEUDO_MAJOR].read = pseudoread;
  devsw[PSEUDO_MAJOR].write = pseudowrite;
}