#include <stdint.h>
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "stat.h"
#include "riscv.h"
#include "defs.h"
#include "device.h"

struct spinlock urandom_lock;
static uint32_t urandom_seed = 12345;

struct spinlock nullstat_lock;
static uint64_t nullstat_count = 0;

int pseudoread(int minor, uint64 addr, int n) {
  switch (minor) {
    case PSEUDO_NULL:
      return 0;
    case PSEUDO_ZERO: {
      for (int i = 0; i < n; i++) {
        char zero = 0;
        if (either_copyout(1, addr + i, &zero, 1) == -1) // Флаг 1 для user
          return -1;
      }
      return n;
    }
    case PSEUDO_URANDOM: {
      acquire(&urandom_lock);
      for (int i = 0; i < n; i++) {
        urandom_seed = urandom_seed * 1664525 + 1013904223;
        uint8_t byte = (urandom_seed >> 16) & 0xFF;
        if (either_copyout(1, addr + i, &byte, 1) == -1) { // Флаг 1
          release(&urandom_lock);
          return -1;
        }
      }
      release(&urandom_lock);
      return n;
    }
    case PSEUDO_NULLSTAT: {
      if (n != sizeof(uint64_t))
        return -1;
      acquire(&nullstat_lock);
      uint64_t count = nullstat_count;
      release(&nullstat_lock);
      if (either_copyout(1, addr, &count, sizeof(count)) == -1) // Флаг 1
        return -1;
      return sizeof(count);
    }
    default: return -1;
  }
}

int pseudowrite(int minor, uint64 addr, int n) {
  switch (minor) {
    case PSEUDO_NULL:
      return n;
    case PSEUDO_ZERO:
      return -1;
    case PSEUDO_URANDOM: {
      if (n != sizeof(urandom_seed))
        return -1;
      uint32_t seed;
      if (either_copyin(&seed, 1, addr, sizeof(seed)) == -1) // Флаг 1
        return -1;
      acquire(&urandom_lock);
      urandom_seed = seed;
      release(&urandom_lock);
      return sizeof(seed);
    }
    case PSEUDO_NULLSTAT: {
      acquire(&nullstat_lock);
      nullstat_count += n;
      release(&nullstat_lock);
      return n;
    }
    default: return -1;
  }
}

void
pseudoinit(void)
{
  initlock(&urandom_lock, "urandom");
  initlock(&nullstat_lock, "nullstat");
  devsw[PSEUDO_MAJOR].read = pseudoread;
  devsw[PSEUDO_MAJOR].write = pseudowrite;
}