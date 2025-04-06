#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/procinfo.h"
#include "user.h"


const char* state_to_str(int state) {
    switch(state) {
        case 0: return "UNUSED ";
        case 1: return "USED   ";
        case 2: return "SLEEP  ";
        case 3: return "RUNNABL";
        case 4: return "RUNNING";
        case 5: return "ZOMBIE ";
        default: return "UNKNOWN";
    }
}

const char* get_parent_name(struct procinfo *procs, int n, int ppid) {
    for(int i = 0; i < n; i++) {
        if(procs[i].pid == ppid)
            return procs[i].name;
    }
    return "-";
}

int main() {
    int cnt = ps_listinfo(0, 0);
    struct procinfo *procs = malloc(cnt * sizeof(struct procinfo));
    int ret = ps_listinfo(procs, cnt);
    
    if(ret != cnt) {
        printf("ps error: %d/%d processes fetched\n", ret, cnt);
        exit(1);
    }
    
    for(int i = 0; i < cnt; i++) {
        printf("pid: %d;\tname: %s;\tstate: %s;\tppid: %d;\tpname: %s\n", 
            procs[i].pid,
            procs[i].name,
            state_to_str(procs[i].state),
            procs[i].ppid,
            get_parent_name(procs, cnt, procs[i].ppid)  
        );
    }
    
    free(procs);
    exit(0);
}
