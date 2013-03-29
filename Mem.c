#include "Mem.h"
#include "Log.h"
#include "Cfg.h"
#include <string.h>		/* memcpy */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
//#include <execinfo.h>
#include <pthread.h>

#define NUM_ALLOCATIONS 1024

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {
  void *ptr;
  const char *func;
  int size;
} allocations[NUM_ALLOCATIONS];


static void backtrace_and_hold(void)
{
#ifndef __ARMEB__
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
#ifndef __ARMEB__
  int i, n;
  void *buffer[32];
  n = backtrace(buffer, 32);
  for (i=0; i < n; i++) {
    fprintf(stderr, "%p\n", buffer[i]);
  }
#endif
  exit(1);
}

static void store(void *ptr, int size, const char *func)
{
  int i;

  for (i=0; i < NUM_ALLOCATIONS; i++) {
    if (allocations[i].ptr == 0L) {
      allocations[i].ptr = ptr;
      allocations[i].size = size;
      allocations[i].func = func;
      return;
    }
  }

  fprintf(stderr, "no more tracking slots left!\n");
  FILE *f = fopen("mem.csv", "w");
  for (i=0; i < NUM_ALLOCATIONS; i++) {
    fprintf(f, "%s, %p, %d,\n", allocations[i].func, allocations[i].ptr, allocations[i].size);
  }
  fclose(f);
  exit(1);
  printf("wtf?\n");
  abort();
  printf("wwwtf?\n");
}

static void clear(void *ptr, const char *func)
{
  int i;

  if (ptr == 0L) {
    abort();
  }

  for (i=0; i < NUM_ALLOCATIONS; i++) {
    if (allocations[i].ptr == ptr) {
      // printf("clear %s %d bytes\n", allocations[i].func, allocations[i].size);
      uint8_t *guardptr = ((uint8_t*)(allocations[i].ptr)) + allocations[i].size;
      uint8_t c = *guardptr;
      if (c != 0xFA) {
	fprintf(stderr, "allocation of %d bytes spilled: %s .. %s [0xFA -> 0x%02X]\n",
		allocations[i].size,
		allocations[i].func, 
		func,
		c);
      }
      allocations[i].ptr = 0L;
      return;
    }
  }

  fprintf(stderr, "allocation for %p not found! (caller %s)\n", ptr, func);
  // backtrace_and_hold();
  // abort();
}


void *_Mem_calloc(int count, int size, const char *func)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = calloc(count, size+1);
  if (cfg.mem_tracking) {
    ((uint8_t*)ptr)[count*size] = 0xFA;
    store(ptr, count*size, func);
  }
  if (cfg.verbosity & 0x10000000) fprintf(stderr, "calloc, %d, %p, %s\n", count*size, ptr, func);
  pthread_mutex_unlock(&mem_lock);
  return ptr;
}

void *_Mem_malloc(int size, const char *func)
{
  pthread_mutex_lock(&mem_lock);
  void *ptr = malloc(size+1);
  if (cfg.mem_tracking) {
    ((uint8_t*)ptr)[size] = 0xFA;
    store(ptr, size, func);
  }
  if (cfg.verbosity & 0x10000000) fprintf(stderr, "malloc, %d, %p, %s\n", size, ptr, func);
  pthread_mutex_unlock(&mem_lock);
  return ptr;
}

void *_Mem_realloc(void *ptr, int newsize, const char *func)
{
  pthread_mutex_lock(&mem_lock);
  if (cfg.mem_tracking) {
    clear(ptr, func);
  }
  void *newptr = realloc(ptr, newsize+1);
  if (cfg.mem_tracking) {
    store(newptr, newsize, func);
    ((uint8_t*)newptr)[newsize] = 0xFA;
  }
  if (cfg.verbosity & 0x10000000) fprintf(stderr, "realloc, %d, %p, %s\n", newsize, ptr, func);
  pthread_mutex_unlock(&mem_lock);
  return newptr;
}


void _Mem_free(void *ptr, const char *func)
{
  pthread_mutex_lock(&mem_lock);
  if (cfg.mem_tracking) {
    clear(ptr, func);
  }
  if (cfg.verbosity & 0x10000000) fprintf(stderr, "free, %p, %s\n", ptr, func);
  free(ptr);
  pthread_mutex_unlock(&mem_lock);
}


void _Mem_memcpy(Ptr dest, int dest_offset, uint8_t *src, int length, const char *func)
{
  if (dest_offset + length < dest.length ) {
    printf("whoa!\n");
    backtrace_and_hold();
  }
  memcpy(dest.data + dest_offset, src, length);
}


int _Mem_unref(MemObject *mo, const char *func)
{
  int rc;
  pthread_mutex_lock(&mem_lock);
  mo->refcount -= 1;
  rc = mo->refcount;
  pthread_mutex_unlock(&mem_lock);
  return rc;
#warning This will probably crash...
}
