#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 1024
char buffer[BUFFER_SIZE];

// tested on mac os

int main(int argc, char *argv[]) {
    int pipefd[2];

    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        close(pipefd[0]);

        for (int i = 1; i < argc; i++) {
            dprintf(pipefd[1], "%s\n", argv[i]);
        }

        close(pipefd[1]);

        waitpid(pid, NULL, 0);
    } else { 
        close(pipefd[1]); 

        ssize_t bytes_read;

        while ((bytes_read = read(pipefd[0], buffer, BUFFER_SIZE))) {
            write(STDOUT_FILENO, buffer, bytes_read);
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    return EXIT_SUCCESS;
}
// .linuxhw/hw1task3.c 