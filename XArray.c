#include "Mem.h"
#include "XArray.h"
#include <string.h>

void _XArray_append(void **elements, void *element_ptr, int element_size, XArrayInfo *info)
{
  if (*elements == 0L) { 
    info->available = 2; 
    *elements = Mem_calloc(info->available, element_size); 
  } 
  else if (info->available == info->count) { 
    info->available *= 2; 
    *elements = Mem_realloc(*elements, info->available * element_size); 
  }
  
  memcpy( ((uint8_t*)(*elements)) + (info->count * element_size), element_ptr, element_size);
  info->count += 1; 
}


extern void _XArray_cleanup(void **elements, XArrayInfo *info)
{
  if (*elements) {
    Mem_free(*elements);
    *elements = NULL;
    info->available = 0;
    info->count = 0;
  }
}
