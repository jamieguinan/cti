#include <stdio.h>
#include <stdlib.h>

static void * _Mem_malloc(int size, const char *func, int line, const char *argtype)
{
  printf("arg=%s\n", arg);
  return NULL;
}

#define Mem_malloc(x) _Mem_malloc(x, __func__, __LINE__, #x)

int main()
{
  Mem_malloc(sizeof(int *));
  return 0;
}
