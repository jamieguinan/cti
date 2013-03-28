#ifndef _CTI_MEM_H_
#define _CTI_MEM_H_

#include <stdint.h>

typedef struct {
  int refcount;
} MemObject;

typedef struct {
  uint8_t *data;
  int length;
} Ptr;

extern void *_Mem_malloc(int size, const char *func);
extern void *_Mem_calloc(int count, int size, const char *func);
extern void *_Mem_realloc(void *ptr, int newsize, const char *func);
extern void _Mem_free(void *ptr, const char *func);
extern void _Mem_memcpy(Ptr dest, int dest_offset, uint8_t *src, int length, const char *func);

#define Mem_malloc(x) _Mem_malloc(x, __func__)
#define Mem_calloc(n, x) _Mem_calloc(n, x, __func__)
#define Mem_realloc(p, x) _Mem_realloc(p, x, __func__)
#define Mem_free(p) _Mem_free(p, __func__)
#define Mem_memcpy(d, o, s, l) _Mem_memcpy(d, o, s, l, __func__)

extern void backtrace_and_exit(void);

#endif
