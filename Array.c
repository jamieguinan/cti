#include "Array.h"
#include "Mem.h"

static void (*Array_error_handler)(void);

void Array_set_error_handler(void (*handler)(void))
{
  Array_error_handler = handler;
}

void Array_grow(void ** items,
	  int itemsize,
	  int * available,
	  int * count)
{
  *count += 1;
  if (*available == 0) {
    *available = 2;
    void *tmp = Mem_malloc(itemsize*(*available));
    if (!tmp && Array_error_handler) { Array_error_handler(); return; }
    (*items) = tmp;
  }
  else if (*count == *available) {
    *available *= 2;
    void *tmp = Mem_realloc((*items), itemsize*(*available));
    if (!tmp && Array_error_handler) { Array_error_handler(); return; }
    (*items) = tmp;
  }
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
