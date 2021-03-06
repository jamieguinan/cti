#ifndef _VIRTUALSTORAGE_H_
#define _VIRTUALSTORAGE_H_

extern void VirtualStorage_init(void);

typedef struct _VirtualStorage VirtualStorage; /* Opaque type. */

extern VirtualStorage * VirtualStorage_from_instance(Instance *pi);
extern void * VirtualStorage_get(VirtualStorage * vs, String * path);

typedef struct {
  void * container;
  /*  For HTTP server access: */
  void * data;
  unsigned long size;
  const char * mime_type;
} Resource;

#endif
