#ifndef _CTI_ARRAY_H_
#define _CTI_ARRAY_H_


/* The thing I like about this module is that separates the list
   growth, which can be kept generic by using pointers to the
   individual list structure members, from the item assignment, which
   is type-specific but can be done with a simple assignment in a
   enclosing macro that also calls the growth function.  My instinct
   had always been to include the growth and assignment in the same
   macro, which C++ templates solve nicely, but can be really messy in
   C, ending up with lots of duplicate code. */

/* Any structure with the same set of member names (with the same types),
   can be used with the accessor macros.  Furthermore, one could call
   Array_grow() directly with any set of compatible variables.  But
   they should be initialized properly before calling. */
extern void Array_grow(void ** items,
                       int itemsize,
                       int * available,
                       int * count);

extern void Array_clear(void ** items,
                        int * available,
                        int * count);

extern void Array_set_error_handler(void (*handler)(void));

#define Array_append(c, item) do { Array_grow((void*)&(c.items), sizeof(c.items[0]), &(c.available), &(c.count)); \
    c.items[c.count-1] = item; } while (0)
#define Array_get(c, i) (c.items[i])
#define Array_delete(c, index) do { if (index < c.count) { int ii; for (ii=index; ii < c.count; ii++) { c.items[ii] = c.items[ii+1]; } c.count -= 1; } } while (0)
#define Array_count(c) (c.count)
#define Array_free(c, type, f)  do { void (*cleanup_func)(type)=f; \
    if (f) { int i; for (i=0;i<c.count;i++){cleanup_func(c.items[i]);} } \
    Array_clear((void*)&(c.items), &c.available, &c.count); } while (0)

#define Array_ptr_append(c, item) do { Array_grow((void*)&(c->items), sizeof(c->items[0]), &(c->available), &(c->count)); \
    c->items[c->count-1] = item; } while (0)
#define Array_ptr_get(c, i) (c->items[i])
#define Array_ptr_count(c) (c->count)
#define Array_ptr_free(c, type, f)  do { void (*cleanup_func)(type)=f; \
    if (f) { int i; for (i=0;i<c->count;i++){cleanup_func(c->items[i]);} } \
    Array_clear((void*)&(c->items), &c->available, &c->count); } while (0)


/* The Array() type declaration at the bottom can be used anywhere:
   local, pointer, struct member, parameter.  */
#define Array(t) struct { t * items; int itemsize; int available; int count; }

#endif
