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

extern void *_Mem_malloc(int size, const char *func, int line, const char * type);
extern void *_Mem_calloc(int count, int size, const char *func, int line, const char * type);
extern void *_Mem_realloc(void *ptr, int newsize, const char *func, int line, const char * type);
extern void _Mem_free(void *ptr, const char *func, int line);
extern void _Mem_memcpy(void *dest, uint8_t *src, int length, const char *func, int line);
extern void _Mem_memcpyPtr(Ptr dest, int dest_offset, uint8_t *src, int length, const char *func, int line);

#define Mem_malloc(x) _Mem_malloc(x, __func__, __LINE__, "anon")
#define Mem_calloc(n, x) _Mem_calloc(n, x, __func__, __LINE__, "anon")
#define Mem_realloc(p, x) _Mem_realloc(p, x, __func__, __LINE__, "anon")
#define Mem_palloc(x) _Mem_calloc(1, sizeof(x), __func__, __LINE__, #x )
#define Mem_free(p) _Mem_free(p, __func__, __LINE__)
#define Mem_memcpy(d, s, l) _Mem_memcpy(d, s, l, __func__, __LINE__)
#define Mem_memcpyPtr(d, o, s, l) _Mem_memcpy(d, o, s, l, __func__, __LINE__)

extern int _Mem_unref(MemObject *mo, const char *func);

#define Mem_unref(mo) _Mem_unref(mo, __func__)

extern void backtrace_and_exit(void);

#endif
