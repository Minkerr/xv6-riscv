#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 1024
char buffer[BUFFER_SIZE];

// tested on mac os

void exit_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int pipefd[2];
    pid_t pid;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <arguments...>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (pipe(pipefd) < 0) {
        exit_error("pipe failed");
    }

    if ((pid = fork()) < 0) {
        exit_error("fork failed");
    }

    if (pid > 0) { 
        
        if (close(pipefd[0]) < 0) {
            exit_error("close read end failed");
        }

        for (int i = 1; i < argc; i++) {
            size_t len = strlen(argv[i]);
            
            
            if (len + 1 >= BUFFER_SIZE) {
                fprintf(stderr, "Argument too long: %s\n", argv[i]);
                close(pipefd[1]);
                exit(EXIT_FAILURE);
            }

            
            memcpy(buffer, argv[i], len);
            buffer[len] = '\n';
            
            
            ssize_t total_written = 0;
            while (total_written < (len + 1)) {
                ssize_t written = write(pipefd[1], 
                                       buffer + total_written,
                                       (len + 1) - total_written);
                if (written < 0) {
                    if (errno == EINTR) continue; 
                    perror("write failed");
                    close(pipefd[1]);
                    exit(EXIT_FAILURE);
                }
                total_written += written;
            }
        }

        
        if (close(pipefd[1]) < 0) {
            exit_error("close write end failed");
        }

        
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            exit_error("waitpid failed");
        }

    } else { 
        
        if (close(pipefd[1]) < 0) {
            exit_error("close write end failed");
        }

        ssize_t bytes_read;
        
        while ((bytes_read = read(pipefd[0], buffer, BUFFER_SIZE)) > 0) {
            ssize_t total_written = 0;
            
            
            while (total_written < bytes_read) {
                ssize_t written = write(STDOUT_FILENO, 
                                        buffer + total_written,
                                        bytes_read - total_written);
                if (written < 0) {
                    if (errno == EINTR) continue;
                    exit_error("write to stdout failed");
                }
                total_written += written;
            }
        }

        
        if (bytes_read < 0) {
            exit_error("read failed");
        }

        
        if (close(pipefd[0]) < 0) {
            exit_error("close read end failed");
        }

        exit(EXIT_SUCCESS);
    }

    return EXIT_SUCCESS;
}
// .linuxhw/hw1task3.c 