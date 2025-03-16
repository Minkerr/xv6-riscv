#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_LEN 512
char buf[MAX_LEN];

int 
main(int argc, char *argv[]) 
{
    int pipefd[2];
    int status;
    int i, len;

    if (pipe(pipefd) < 0) {
        printf("pipe has failed\n");
        exit(1);
    }

    int pid = fork();

    if (pid < 0) {
        printf("fork has failed\n");
        exit(1);
    }
    
    if (pid == 0) {  

        if (close(pipefd[1]) < 0) {
            printf("failed to close write end\n");
            exit(1);
        }
        
        if (close(0) < 0) {
            printf("failed to close stdin\n");
            exit(1);
        }
        
        if (dup(pipefd[0]) != 0) {
            printf("dup has failed\n");
            exit(1);
        }
        
        if (close(pipefd[0]) < 0) {
            printf("failed to close pipefd[0]\n");
            exit(1);
        }

        char *args[] = { "wc", 0 };
        exec("/wc", args);

        printf("exec wc has failed\n");
        exit(1);
    } 
    else { 

        if (close(pipefd[0]) < 0) {
            printf("failed to close read end\n");
            exit(1);
        }
        
        for (i = 1; i < argc; i++) {
            len = strlen(argv[i]);
            if (len >= MAX_LEN - 1) {  
                printf("argument is too long\n");
                if (close(pipefd[1]) < 0) {
                    printf("failed to close pipe after error\n");
                }
                wait(&status);
                exit(1);
            }
            
            
            memcpy(buf, argv[i], len);
            buf[len] = '\n';
            
            
            int remaining = len + 1;
            char *p = buf;
            while (remaining > 0) {
                int written = write(pipefd[1], p, remaining);
                if (written < 0) {
                    printf("write error\n");
                    if (close(pipefd[1]) < 0) {
                        printf("failed to close pipe after error\n");
                    }
                    wait(&status);
                    exit(1);
                }
                remaining -= written;
                p += written;
            }
        }
        
        if (close(pipefd[1]) < 0) {
            printf("failed to close write end in parent\n");
            wait(&status);
            exit(1);
        }
        
        wait(&status);
        exit(0);
    }
}