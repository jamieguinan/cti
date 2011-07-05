#include <stdio.h>
#include <string.h>
#include "CTI.h"
#include "File.h"


ArrayU8 * File_load_data(const char *filename)
{
  long len;
  long n;
  FILE *f = fopen(filename, "rb");

  if (!f) {
    perror(filename);
    return 0L;
  }

  if (strncmp("/proc/", filename, strlen("/proc/")) == 0) {
    len = 32768;
  }
  else {
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
  }

  ArrayU8 *a = ArrayU8_new();
  ArrayU8_extend(a, len);
  n = fread(a->data, 1, len, f);
  if (n < len) {
    fprintf(stderr, "warning: only read %ld of %ld expected bytes from %s\n", n, len, filename);
  }
  a->data[n] = 0;

  fclose(f);

  return a;
}


String * File_load_text(const char *filename)
{
  ArrayU8 *a = File_load_data(filename);
  String *s = ArrayU8_to_string(&a);
  return s;
}

