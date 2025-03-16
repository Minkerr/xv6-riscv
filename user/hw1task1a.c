#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid = fork();

  if (pid < 0) {
    printf("fork has failed\n");
    exit(1);
  }

  if (pid == 0) {

    printf("Child is sleeping for 10 seconds\n");
    sleep(100); 
    exit(1);
    
  } else { 
    printf("Parent pid: %d\n", getpid());
    printf("Child pid: %d\n", pid);
    
    int status;
    int w_pid = wait(&status);
    
    if (w_pid != pid) {
      printf("wait error: expected %d, got %d\n", pid, w_pid);
      exit(1);
    }
    
    printf("Child %d has exited\n", pid);
    exit(0);
  }
}