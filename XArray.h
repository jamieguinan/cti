#ifndef _XARRAY_H_
#define _XARRAY_H_

typedef struct {
  void (*cleanup)(void *data);
  int item_size;
  int available;
  int count;
  int error;			/* Error state. */
} XArrayInfo;

extern void _XArray_append(void **elements, void *element_ptr, int element_size, XArrayInfo *info);
extern void _XArray_cleanup(void **elements, XArrayInfo *info);

/* Other approach */

/* Declaration.  Can use within structs, or bare. */
#define XArray(type, name) type *name; XArrayInfo name ## _info

/* Note: "name" is a pointer variable, and is modified in-place by the
   append, delete, and other(?) functions, by passing its address.  So
   if "name" is passed to another function, and that function calls
   one of those functions, it will be modifying a copy of the pointer
   variable, and not the pointer variable itself.  Be careful.
   */
/* "name" should be a "type *" where "type" was used with XArray(). */

#define XArray_append(name, element_ptr)  _XArray_append((void**)&name, element_ptr, sizeof(name[0]), &name ## _info)

#define XArray_cleanup(name)  _XArray_cleanup((void**)&name,  &name ## _info)

#define XArray_count(name)  ( name ## _info.count )
#define XArray_get_ptr(name, i, ptr)  ptr = &name[i]

#endif
