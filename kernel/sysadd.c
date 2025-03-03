#include "types.h"
#include "param.h"
#include "stat.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

uint64 sys_add(void)
{
    int a, b;

    argint(0, &a);
    argint(1, &b);

    return a + b;
}