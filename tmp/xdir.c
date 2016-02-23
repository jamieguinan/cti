#include <stdio.h>
#include <stdint.h>

int main()
{
  uint8_t line[640];
  int i;
  char buffer[256];
  FILE *f;
  f = fopen("000000000.pgm", "rb");
  fgets(buffer, sizeof(buffer), f);
  fgets(buffer, sizeof(buffer), f);
  fgets(buffer, sizeof(buffer), f);

  fread(line, sizeof(line), 1, f);

  enum { NONE, UP, DOWN};
  char* dirs[3] = {"NONE", "UP", "DOWN"};
  int d = NONE;
  for (i=0; i < 600; i++) {
    int last_d = d;
    if (line[i] < line[i+1]) {
      d = UP;
    }
    else if (line[i] > line[i+1]) {
      d = DOWN;
    }
    if (d != last_d) {
      printf("%d: %s\n", i, dirs[d]);
    }
  }

  return 0;
}
