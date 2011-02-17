#include "Collection.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  int n = 1000;
  int i;

  if (argc == 2) {
    n = atoi(argv[1]);
  }
  
  printf("n=%d\n", n);

  struct {
    void *addr;
    const char *label;
  } * sym_table = 0L;
  Collection c_sym_table;

  Collection_init(sym_table, c_sym_table);


  uint8_t *bytes;
  Collection c_bytes;
  Collection_init(bytes, c_bytes);

  //putlast(0xff, bytes, &c_bytes);
  //Collection_prepare_putlast(&c_bytes);
  //bytes[(&c_bytes)->iNextFree++] = 0xff;

  for (i=0; i < n; i++) {
    putlast((i & 0xff), bytes, &c_bytes);
  }

  printf("%d entries in bytes\n", c_bytes.count);
  
  uint32_t *words;
  Collection words_c;
  uint32_t v32;
  Collection_init(words, words_c);
  for (i=0; i < n; i++) {
    putlast((i & 0xffffffff), words, &words_c);
    if ((i+1) % 100 == 0) {      
      takefirst(v32, words, &words_c);
      printf("took %d\n", v32);
    }
  }
  
  printf("%d entries in words\n", words_c.count);

  return 0; 
}
