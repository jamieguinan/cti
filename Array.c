#include "Array.h"
#include "Mem.h"

void Array_grow(void ** items,
	  int itemsize,
	  int * available,
	  int * count) 
{
  *count += 1;
  //printf("*available=%d\n", *available);
  if (*available == 0) {
    *available = 2;
    void *tmp = Mem_malloc(itemsize*(*available));
    /* FIXME: if (!tmp) { error_handler(); return; } */
    (*items) = tmp;
  }
  else if (*count == *available) {
    *available *= 2;
    void *tmp = Mem_realloc((*items), itemsize*(*available));
    /* FIXME: if (!tmp) { error_handler(); return; } */
    (*items) = tmp;
  }
  // printf("*available=%d\n", *available);
}


void Array_clear(void ** items,
		 int * available,
		 int * count)
{
  if (*items) {
    Mem_free(*items);
  }
  (*items) = 0L;
  *available = 0;
  *count = 0;
}
