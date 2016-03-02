#include <stdio.h>


void x(void)
{
  void * bfa;
  void * bra;
  unsigned int lvl = 0;
  bfa = __builtin_frame_address(lvl);
  bra = __builtin_frame_address(lvl);
  printf("%d: %p\n", lvl, bfa);
  printf("%d: %p\n", lvl, bra);
}

int main()
{
  x();
  return 0;
}
