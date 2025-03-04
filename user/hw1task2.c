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
        printf("fork has failed");
        exit(-1);
    }
    

    if (pid == 0) {  
        close(pipefd[1]);  
        close(0);
        
        if (dup(pipefd[0]) != 0) {
            printf("dup has failed\n");
            exit(1);
        }
        
        close(pipefd[0]);


        char *args[] = { "wc", 0 };
        exec("/wc", args);

        printf("exec wc has failed\n");
        exit(1);
    } 
    else { 

        close(pipefd[0]);
        
        for (i = 1; i < argc; i++) {

            len = strlen(argv[i]);

            if (len >= MAX_LEN) {
                printf("argument is too long\n");
                close(pipefd[1]);
                wait(&status);
                exit(1);
            }
            
            strcpy(buf, argv[i]);
            buf[len] = '\n';

            if (write(pipefd[1], buf, len + 1) != len + 1) {
                printf("write has failed\n");
                close(pipefd[1]);
                wait(&status);
                exit(1);
            }
        }
        
        close(pipefd[1]);
        wait(&status);
        exit(0);
    }
}