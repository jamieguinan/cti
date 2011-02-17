#include <stdio.h>

void foo(void **ptr)
{
  char *p = *ptr;
  *ptr = 0L;
  printf("foo: %p %s\n", p, p);
}


int main(int argc, char *argv[])
{
  char *p = argv[0];
  printf("%p %s\n", p, p);
  foo((void **)&p);
  printf("%p\n", p);
  return 0;
}
