#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

  char msg[] = "Hello world (write)\n";
  write(1, msg, sizeof(msg) - 1); // Excluding trailing \0

  printf("Hello world (printf)\n");
  
  exit(0);
}