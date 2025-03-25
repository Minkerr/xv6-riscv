#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int m = mutex(); 

    if (fork() == 0) {
        for (int i = 1; i < argc; i++) {
            for (char *c = argv[i]; *c; c++) {
                printf("%d: arg %d, char '%c'\n", getpid(), i, *c);
            }
        }
        exit(0);
    }
    
    for (int i = 1; i < argc; i++) {
        for (char *c = argv[i]; *c; c++) {
            printf("%d: arg %d, char '%c'\n", getpid(), i, *c);
        }
    }
    wait(0);
    
    if (fork() == 0) {
        for (int i = 1; i < argc; i++) {
            for (char *c = argv[i]; *c; c++) {
                mutex_lock(m);
                printf("%d: arg %d, char '%c'\n", getpid(), i, *c);
                mutex_unlock(m);
            }
        }
        exit(0);
    }
    
    for (int i = 1; i < argc; i++) {
        for (char *c = argv[i]; *c; c++) {
            mutex_lock(m);
            printf("%d: arg %d, char '%c'\n", getpid(), i, *c);
            mutex_unlock(m);
        }
    }
    wait(0);
    close(m);
    exit(0);
}