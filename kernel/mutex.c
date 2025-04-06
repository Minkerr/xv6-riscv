#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

static int mutex_counter = 0;
static struct spinlock mutex_counter_lock; 

void init_mutex_subsystem() {
  initlock(&mutex_counter_lock, "mutex_counter");
}

struct file* mutexalloc() {
  init_mutex_subsystem();
  
  struct file *f = filealloc();
  if(!f) return 0;

  struct sleeplock *sl = (struct sleeplock*)kalloc();
  
  acquire(&mutex_counter_lock);
  sl->mid = ++mutex_counter;
  release(&mutex_counter_lock);
  
  printf("mutexalloc: created mutex#%d\n", sl->mid); 

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
  printf("mutexclose: destroying mutex#%d\n", f->mutex->mid);
  kfree(f->mutex);
  f->mutex = 0;
}
