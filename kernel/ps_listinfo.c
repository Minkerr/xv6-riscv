#include "types.h"
#include "param.h"
#include "stat.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "syscall.h"
#include "procinfo.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;

uint64 
sys_ps_listinfo(void) {
    int addr_arg;
    int lim_arg;
    struct procinfo *plist;
    struct proc *p = myproc();
    uint64 copied = 0;

    argint(0, &addr_arg);
    argint(1, &lim_arg);

    if (addr_arg == 0) {
        for (int i = 0; i < NPROC; i++) {
            acquire(&proc[i].lock);
            if (proc[i].state != UNUSED) copied++;
            release(&proc[i].lock);
        }
        return copied;
    }

    plist = (struct procinfo*)((uint64)addr_arg);
    if (lim_arg <= 0 || (uint64)plist >= p->sz || 
        (uint64)plist + sizeof(struct procinfo)*lim_arg > p->sz) {
        return -1;
    }

    for (int i = 0; i < NPROC && copied < (uint64)lim_arg; i++) {
        acquire(&proc[i].lock);
        if (proc[i].state == UNUSED) {
            release(&proc[i].lock);
            continue;
        }

        struct procinfo info;
        info.pid = proc[i].pid;
        safestrcpy(info.name, proc[i].name, sizeof(info.name));
        info.state = proc[i].state;

        acquire(&wait_lock);
        info.ppid = proc[i].parent ? proc[i].parent->pid : 0;
        release(&wait_lock);

        uint64 dest = (uint64)plist + copied * sizeof(struct procinfo);
        if (copyout(p->pagetable, dest, (char*)&info, sizeof(info)) < 0) {
            release(&proc[i].lock);
            return -1;
        }

        copied++;
        release(&proc[i].lock);
    }

    return copied;
}