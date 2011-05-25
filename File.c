#include <stdio.h>
#include "CTI.h"
#include "File.h"

ArrayU8 * File_load_data(const char *filename)
{
  long len;
  FILE *f = fopen(filename, "rb");

  if (!f) {
    perror(filename);
    return 0L;
  }

  fseek(f, 0, SEEK_END);
  len = ftell(f);
  fseek(f, 0, SEEK_SET);

  ArrayU8 *a = ArrayU8_new();
  ArrayU8_extend(a, len);
  if (fread(a->data, len, 1, f) != 1) {
    perror("fread");
  }

  fclose(f);

  return a;
}


String * File_load_text(const char *filename)
{
  ArrayU8 *a = File_load_data(filename);
  String *s = ArrayU8_to_string(&a);
  return s;
}

