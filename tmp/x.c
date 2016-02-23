#include <stdio.h>
#include <stdint.h>

int main()
{
  FILE *f;
  uint8_t input[640*480*2];
  uint8_t y[640*480];
  uint8_t cb[640*480/2];
  uint8_t cr[640*480/2];

  f = fopen("snap13479.y422p", "rb");
  fread(input, sizeof(input), 1, f);
  fclose(f);

  uint8_t *p = input;
  int iy = 0;
  int icb = 0;
  int icr = 0;
  int i;
  
  for (i=0; i < sizeof(input)/4; i++) {
    y[iy++] = *p++;
    cb[icb++] = *p++;
    y[iy++] = *p++;
    cr[icr++] = *p++;
  }

  f = fopen("out.y422p", "wb");
  fwrite(y, sizeof(y), 1, f);
  fwrite(cb, sizeof(cb), 1, f);
  fwrite(cr, sizeof(cr), 1, f);
  fclose(f);

  return 0;
}
