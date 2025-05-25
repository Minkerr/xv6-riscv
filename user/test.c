#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void
test_null(void)
{
  int fd;
  char buf[10];
  int n;
  
  printf("testing null device:\n");
  fd = open("null", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  n = write(fd, "test", 4);
  printf("%d\n\n", n);
  close(fd);
}

void
test_zero(void)
{
  int fd;
  char buf[10];
  int n, i;
  
  printf("testing zero device:\n");
  fd = open("zero", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  for(i = 0; i < n; i++) {
    printf("%d ", buf[i]);
  }
  printf("\n");
  n = write(fd, "test", 4);
  printf("%d\n\n", n);
  close(fd);
}

void
test_urandom(void)
{
  int fd;
  char buf[10];
  int n, i;
  uint seed = 0x12345678;
  
  printf("testing urandom device:\n");
  fd = open("urandom", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  for(i = 0; i < n; i++) {
    printf("0x%x ", buf[i] & 0xff);
  }
  printf("\n");
  n = write(fd, &seed, sizeof(seed));
  printf("%d\n", n);
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  for(i = 0; i < n; i++) {
    printf("0x%x ", buf[i] & 0xff);
  }
  printf("\n\n");
  close(fd);
}

void
test_nullstat(void)
{
  int fd;
  uint64 count;
  int n;
  
  printf("testing nullstat device:\n");
  fd = open("nullstat", O_RDWR);
  if(fd < 0) {
    printf("Failed to open nullstat device\n");
    return;
  }
  
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  n = write(fd, "test", 4);
  printf("%d\n", n);
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  n = write(fd, "more data", 9);
  printf("%d\n", n);
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  n = read(fd, &count, 1);
  printf("%d\n", n);
  close(fd);
}

int
main(void)
{
  test_null();
  test_zero();
  test_urandom();
  test_nullstat();
  
  exit(0);
}