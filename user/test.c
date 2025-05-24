#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define ARRAY_PAGES 3

int global_var = 64;

void print_separator(void) {
  printf("----------------------------------------\n");
}

int
main(int argc, char *argv[])
{
  int stack_var = 100;
  int *heap_array;
  int i;

  print_separator();

  pageinfo(0, 0, 0);
  print_separator();

  printf("\n\n");
  heap_array = (int*)malloc(ARRAY_PAGES * PGSIZE);
  if (heap_array == 0) {
    printf("malloc failed\n");
    exit(1);
  }

  for (i = 0; i < (ARRAY_PAGES * PGSIZE / sizeof(int)); i++) {
    heap_array[i] = i;
  }

  printf("\n\n");
  pageinfo(0, 0, 0);
  print_separator();

  printf("\n\n");
  clearflags(0, 0, 3);
  
  printf("\n\n");
  pageinfo(0, 0, 0);
  print_separator();

  printf("\n\n");
  pageinfo(&global_var, sizeof(global_var), 2);
  print_separator();

  printf("\n\n");
  pageinfo(&stack_var, sizeof(stack_var), 2);
  print_separator();

  printf("\n\n");
  pageinfo(heap_array, ARRAY_PAGES * PGSIZE, 2);
  print_separator();

  printf("\n\n");
  clearflags(0, 0, 3);
  
  global_var = 63;
  
  printf("\n\n");
  pageinfo(&global_var, sizeof(global_var), 3);
  print_separator();

  printf("\n\n");
  stack_var = 101;
  
  printf("\n\n");
  pageinfo(&stack_var, sizeof(stack_var), 3); 
  print_separator();

  printf("\n\n");
  for (i = 0; i < (ARRAY_PAGES * PGSIZE / sizeof(int)); i++) {
    heap_array[i] = i * 2;
  }
  
  printf("\n\n");
  pageinfo(heap_array, ARRAY_PAGES * PGSIZE, 3);
  print_separator();

  printf("\n\n");
  free(heap_array);
  
  printf("\n\n");
  pageinfo(0, 0, 0);
  print_separator();

  printf("\n\n");
  print_separator();
  print_separator();
  print_separator();
  exit(0);
}