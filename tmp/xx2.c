#include <stdio.h>
#include <stdint.h>


int main(int argc, char *argv[])
{
  const uint8_t *ptr;
  int n;
  { const uint8_t tmp[] = { 'a', 'b', 'c', 'd', 'e'}; ptr = tmp; n = sizeof(tmp); }

  printf("n=%d\n", n);

  return 0;
}
