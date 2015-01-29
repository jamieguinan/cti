#ifndef _CTI_ARRAY_H_
#define _CTI_ARRAY_H_

/* The Array() type declaration at the bottom can be used anywhere:
   local, pointer, struct member, parameter.  Additionally, any
   structure with the same set of member names (with the same
   types), can be used with the accessor macros.   Furthermore,
   one could call Array_grow() directly with any set of compatible
   variables. */

extern void Array_grow(void ** items,
		       int itemsize,
		       int * available,
		       int * count);

#define Array_append(item, c) { Array_grow((void*)&(c.items), sizeof(c.items[0]), &(c.available), &(c.count)); \
  c.items[c.count-1] = item; }

#define Array_get(c, i) (c.items[i])

#define Array_ptr_append(item, c) { Array_grow((void*)&(c->items), sizeof(c->items[0]), &(c->available), &(c->count)); \
  c->items[c->count-1] = item; }

#define Array_ptr_get(c, i) (c->items[i])

#define Array(t) struct { t * items; int itemsize; int available; int count; }

#endif
