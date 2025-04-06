#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/param.h"
#include "kernel/spinlock.h"
#include "kernel/procinfo.h"
#include "kernel/proc.h"
#include "user/user.h"


int main() {
    int cnt = ps_listinfo(0, 0);
    printf("Total processes: %d\n", cnt);

    struct procinfo plist[64];
    int ret = ps_listinfo(plist, 64);
    if (ret < 0) {
        printf("Error retrieving process list\n");
    } else {
        for (int i = 0; i < ret; i++) {
            printf("PID: %d, Name: %s, State: %d, PPID: %d\n", plist[i].pid, plist[i].name, plist[i].state, plist[i].ppid);
        }
    }

    exit(0);
}