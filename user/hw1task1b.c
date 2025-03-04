#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid = fork();
  if (pid < 0) {
    printf("fork has failed");
    exit(-1);
  }

  if (pid == 0) { 

      printf("Child is sleeping for 5 seconds\n");
      sleep(100); 
      exit(1);

  } else { 
      printf("Parent pid: %d\n", getpid());
      printf("Child pid: %d\n", pid);
      
      kill(pid);

      int status;
      wait(&status);
      printf("Child %d has exited\n", pid);
      
      exit(0);
  }
}

