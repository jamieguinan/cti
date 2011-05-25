#include <stdio.h>
#include <sys/time.h>

int main()
{
  struct timeval tv;

  printf("sizes...\n");
  printf("timeval.tv_sec: %ld (time_t)\n", sizeof(tv.tv_sec));
  printf("timeval.tv_usec: %ld (suseconds_t)\n", sizeof(tv.tv_usec));

  return 0;
}

