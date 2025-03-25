#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void read_write_test() {
    int m = mutex();
    char buf[10];
    
    if (read(m, buf, 1) > 0) 
        printf("read succeeded\n");
    if (write(m, "x", 1) > 0) 
        printf("write succeeded\n");
    
    close(m);
}

void close_while_locked() {
    int m = mutex();
    mutex_lock(m);
    
    if (fork() == 0) {
        sleep(10); 
        if (mutex_unlock(m) < 0)
            printf("unlock failed in child\n");
        exit(0);
    }
    
    close(m); 
    wait(0);
}

int main() {
    printf("read write test:\n");
    read_write_test();
    
    printf("\nclose locked test:\n");
    close_while_locked();
    
    exit(0);
}