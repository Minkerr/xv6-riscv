#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/procinfo.h"
#include "user.h"

void test_null() {
    int cnt = ps_listinfo(0, 0);
    printf("Total processes: %d\n", cnt);
}

void test_invalid() {
    int ret = ps_listinfo((struct procinfo*)0xDEADBEEF, 10);
    if(ret < 0) 
        printf("You can't write to invalid adress\n");
}


void test_normal() {
    int cnt = ps_listinfo(0, 0);
    struct procinfo *buf = malloc(cnt * sizeof(struct procinfo));
    int ret = ps_listinfo(buf, cnt);
    if(ret == cnt) {
        printf("test\n");   
    }
    free(buf);
}


int main() {
    test_null();
    test_invalid();
    test_normal();
    exit(0);
}