#include "Mem.h"
#include "Cfg.h"
#include <string.h>		/* memcpy */
#include <stdlib.h>
#include <inttypes.h>		/* PRIu64 */
#include <unistd.h>
#include <stdio.h>
//#include <execinfo.h>
#include <pthread.h>

#include "StackDebug.h"

#define NUM_ALLOCATIONS 1024

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

static void backtrace_and_hold(void)
{
#if 0
  /* This doesn't work on ARM, nor n270. */
  int i, n;
  void *buffer[32];
  n = backtrace(buffer, 32);
  for (i=0; i < n; i++) {
    fprintf(stderr, "%p\n", buffer[i]);
  }

  Log_dump();
#endif
  while(1) { sleep (1); }
}


void backtrace_and_exit(void)
{
#if 0
  int i, n;
  void *buffer[32];
  n = backtrace(buffer, 32);
  for (i=0; i < n; i++) {
    fprintf(stderr, "%p\n", buffer[i]);
  }
#endif
  exit(1);
}


void *_Mem_calloc(int count, int size, const char *func, int line)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = calloc(count, size);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "+");
  }
  pthread_mutex_unlock(&mem_lock);
  // fprintf(stderr, "calloc: %p\n", ptr);
  return ptr;
}

void *_Mem_malloc(int size, const char *func, int line)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = malloc(size);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "+");
  }
  pthread_mutex_unlock(&mem_lock);
  // fprintf(stderr, "malloc: %p\n", ptr);
  return ptr;
}

void *_Mem_realloc(void *ptr, int newsize, const char *func, int line)
{
  pthread_mutex_lock(&mem_lock);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "-");
  }
  void *newptr = realloc(ptr, newsize);
  if (cfg.mem_tracking) {
    StackDebug2(newptr, "+");
  }
  pthread_mutex_unlock(&mem_lock);
  return newptr;
}


void _Mem_free(void *ptr, const char *func, int line)
{
  pthread_mutex_lock(&mem_lock);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "-");
  }
  // fprintf(stderr, "free: %p\n", ptr);
  free(ptr);
  pthread_mutex_unlock(&mem_lock);
}


void _Mem_memcpy(void *dest, uint8_t *src, int length, const char *func, int line)
{
  memcpy(dest, src, length);
}


void _Mem_memcpyPtr(Ptr dest, int dest_offset, uint8_t *src, int length, const char *func, int line)
{
  if (dest_offset + length < dest.length ) {
    printf("whoa!\n");
    backtrace_and_hold();
  }
  memcpy(dest.data + dest_offset, src, length);
}


#if 0
/* This is unused. */
int _Mem_unref(MemObject *mo, const char *func)
{
  int rc;
  pthread_mutex_lock(&mem_lock);
  mo->refcount -= 1;
  rc = mo->refcount;
  pthread_mutex_unlock(&mem_lock);
  return rc;
}
#endif
