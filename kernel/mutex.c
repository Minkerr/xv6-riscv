#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

struct file* mutexalloc() {
  struct file *f = filealloc();
  if(!f) return 0;
  
  struct sleeplock *sl = (struct sleeplock*)kalloc();
  printf("mutexalloc: %p\n", sl);

  if(!sl) {
    fileclose(f);
    return 0;
  }
  
  initsleeplock(sl, "mutex");
  f->type = FD_MUTEX;
  f->mutex = sl;
  f->readable = f->writable = 0;
  return f;
}

void mutexclose(struct file *f) {
    printf("mutexclose: %p\n", f->mutex);
  kfree(f->mutex);
  f->mutex = 0;
}