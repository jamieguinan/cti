#include <stdio.h>

void foo(void **ptr)
{
  char *p = *ptr;
  *ptr = 0L;
  printf("foo: %p %s\n", p, p);
}

#define count_args(a, ...)  &(ArrayU8) { }

int main(int argc, char *argv[])
{

#define ArrayU8_append_bytes(a, ...)  Array_append(ArrayU8 *a, & (ArrayU8) { .data = { __VA_ARGS__ }, .len = n, .available = n } 

  char *p = argv[0];
  printf("%p %s\n", p, p);
  foo((void **)&p);
  printf("%p\n", p);
  return 0;
}
