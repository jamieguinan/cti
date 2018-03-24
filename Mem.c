#include "Mem.h"
#include "Cfg.h"
#include "dpf.h"
#include <string.h>		/* memcpy */
#include <stdlib.h>
#include <inttypes.h>		/* PRIu64 */
#include <unistd.h>
#include <stdio.h>
//#include <execinfo.h>
#include <pthread.h>

#include "StackDebug.h"

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

#define NUM_ALLOCATIONS 10000

static struct {
  unsigned long total;
  int active;
  struct {
    void * ptr;
    int size;
    const char * func;
    int line;
  } allocations[NUM_ALLOCATIONS];
  int top;
} Mem = {};

static void mt3(void * ptr, int size, const char * func, int line)
{
  int i;
  if (size < 0) {
    /* free or realloc */
    for (i=Mem.top-1; i >= 0; i--) {
      if (ptr == Mem.allocations[i].ptr) {
	Mem.total -= Mem.allocations[i].size;
	Mem.active -= 1;
	Mem.allocations[i].ptr = NULL;
	Mem.allocations[i].size = 0;
	break;
      }
    }
  }
  else {
    /* malloc or calloc */
    for (i=Mem.top-1; i >= 0; i--) {
      if (Mem.allocations[i].ptr == NULL) {
	break;
      }
    }
    if (i == -1) {
      Mem.top += 1;
      i = Mem.top;
    }
    Mem.total += size;
    Mem.active += 1;
    Mem.allocations[i].ptr = ptr;
    Mem.allocations[i].size = size;
    Mem.allocations[i].func = func;
    Mem.allocations[i].line = line;
  }

  {
    static int x = 0;
    if (x % 100 == 0) {
      dpf("mt3 top=%d total=%ld active=%d\n", Mem.top, Mem.total, Mem.active);
    }
    x += 1;
  }
}

void mdump(void)
{
  int i;
  const char * path = "/dev/shm/mdump.txt";
  FILE * f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "could not open %s\n", path);
    return;
  }
  fprintf(stderr, "dumping to %s...", path);  fflush(stderr);
  pthread_mutex_lock(&mem_lock);
  for (i=0; i < Mem.top; i++) {
    if (Mem.allocations[i].ptr) {
      fprintf(f,"%p %d %s:%d\n"
	      , Mem.allocations[i].ptr
	      , Mem.allocations[i].size
	      , Mem.allocations[i].func
	      , Mem.allocations[i].line
	      );
    }
  }
  fprintf(stderr, "done\n");
  fclose(f);
  pthread_mutex_unlock(&mem_lock);
}

static void backtrace_and_hold(void)
{
#if 0
  /* This doesn't work on ARM, nor Atom n270. */
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


void *_Mem_calloc(int count, int size, const char *func, int line, const char * type)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = calloc(count, size);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "+");
  }
  if (cfg.mem_tracking_3) {
    mt3(ptr, count*size, func, line);
  }
  pthread_mutex_unlock(&mem_lock);
  // fprintf(stderr, "calloc: %p\n", ptr);
  return ptr;
}

void *_Mem_malloc(int size, const char *func, int line, const char * type)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = malloc(size);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "+");
  }
  if (cfg.mem_tracking_3) {
    mt3(ptr, size, func, line);
  }
  pthread_mutex_unlock(&mem_lock);
  // fprintf(stderr, "malloc: %p\n", ptr);
  return ptr;
}

void *_Mem_realloc(void *ptr, int newsize, const char *func, int line, const char * type)
{
  pthread_mutex_lock(&mem_lock);
  if (cfg.mem_tracking) {
    StackDebug2(ptr, "-");
  }
  if (cfg.mem_tracking_3) {
    mt3(ptr, -2, func, line);
  }
  void *newptr = realloc(ptr, newsize);
  if (cfg.mem_tracking) {
    StackDebug2(newptr, "+");    
  }
  if (cfg.mem_tracking_3) {
    mt3(newptr, newsize, func, line);
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
  if (cfg.mem_tracking_3) {
    mt3(ptr, -1, func, line);
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
