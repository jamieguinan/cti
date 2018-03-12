/*
 * This module is to help debug programs by dumping the call stack
 * when StackDebug() is called.
 * The cyg_profile_*() functions are only called if CTI is built with
 * INSTRUMENT enabled in the Makefile.  There is a special rule
 * to avoid instrumenting this module, to avoid recursion.
 */

#include "CTI.h"
#include <stdio.h>		/* FILE, sprintf, printf */
#include <string.h>		/* strstr */

__attribute__((no_instrument_function))
void __cyg_profile_func_enter (void *this_fn,
			       void *call_site)
{
  if (!instance_key_initialized) { return ; }

  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { return; }

  if (pi->stack_index < CTI_INSTANCE_STACK_MAX) {
    pi->stack[pi->stack_index] = this_fn;
    pi->stack_index += 1;
  }
}


__attribute__((no_instrument_function))
void __cyg_profile_func_exit  (void *this_fn,
			       void *call_site)
{
  if (!instance_key_initialized) { return ; }

  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { return; }

  if (pi->stack_index > 0) {
    pi->stack_index -= 1;
  }
}

static void show_symbol(void *addr)
{
  FILE *f = fopen("/tmp/cti.map", "r");
  if (!f) {
    fprintf(stderr, " %p\n", addr);
    return;

  }
  char addr_str[64];
  sprintf(addr_str, "%016lx ", (long)addr);
  //printf("searching for %s\n", addr_str);
  while (1) {
    char line[256];
    if (fgets(line, sizeof(line), f) == NULL) {
      break;
    }
    if (strstr(line, addr_str)) {
      fprintf(stderr, "%s", line);
      break;
    }
  }
  fclose(f);
}

void StackDebug(void)
{
  int i;
  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { return; }
  fprintf(stderr, "%s:\n", pi->label);
  if (pi->stack_index) {
    for (i=0; i < pi->stack_index; i++) {
      show_symbol(pi->stack[i]);
    }
  }
  else {
    fprintf(stderr, "%s: this build probably did not have instrumented functions\n", __func__);
  }
}

void StackDebug2(void *ptr, const char * text)
{
  int i;
  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { 
    fprintf(stderr, "no instance key for thread\n");
    return; 
  }
  fprintf(stderr, "%s::", pi->label);
  for (i=0; i < pi->stack_index; i++) {
    fprintf(stderr, "/%p", pi->stack[i]);
  }
  fprintf(stderr, "::%p::%s", ptr, text);
  fprintf(stderr, "\n");
}


void StackDebug3up(void *ptr, const char *type, ssize_t size)
{
  int i;
  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { 
    fprintf(stderr, "no instance key for thread\n");
    return; 
  }
  fprintf(stderr, "%s::", pi->label);
  for (i=0; i < pi->stack_index; i++) {
    fprintf(stderr, "/%p", pi->stack[i]);
  }
  fprintf(stderr, "::%p::%s", ptr, type);
  fprintf(stderr, "\n");
}


void StackDebug3down(void *ptr)
{
  int i;
  Instance *pi = pthread_getspecific(instance_key);
  if (!pi) { 
    fprintf(stderr, "no instance key for thread\n");
    return; 
  }
  fprintf(stderr, "%s::", pi->label);
  for (i=0; i < pi->stack_index; i++) {
    fprintf(stderr, "/%p", pi->stack[i]);
  }
  fprintf(stderr, "::%p", ptr);
  fprintf(stderr, "\n");
}
