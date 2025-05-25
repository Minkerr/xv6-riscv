#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void
hexdump(int fd, int n)
{
  int i, r;
  char buf[16];
  
  while(n > 0) {
    r = read(fd, buf, n < 16 ? n : 16);
    if(r <= 0) {
      break;
    }
    
    for(i = 0; i < r; i++) {
      if(i % 16 == 0) {
        printf("%d: ", i);
      }
      printf("0x%x ", buf[i] & 0xff);
      if((i + 1) % 16 == 0) {
        printf("\n");
      }
    }
    
    if(r % 16 != 0) {
      printf("\n");
    }
    
    n -= r;
  }
}

int
main(int argc, char *argv[])
{
  int fd;
  int n;
  
  if(argc != 3) {
    fprintf(2, "error\n");
    exit(1);
  }
  
  n = atoi(argv[2]);
  if(n <= 0) {
    fprintf(2, "invalid bytes: %s\n", argv[2]);
    exit(1);
  }
  
  if((fd = open(argv[1], O_RDONLY)) < 0) {
    fprintf(2, "cannot open %s\n", argv[1]);
    exit(1);
  }
  
  hexdump(fd, n);
  
  close(fd);
  exit(0);
}