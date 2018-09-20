/*
 * This module is to help debug programs by dumping the call stack
 * when StackDebug() is called.
 * The cyg_profile_*() functions are only called if CTI is built with
 * INSTRUMENT enabled in the Makefile.  There is a special rule
 * to avoid instrumenting this module, to avoid recursion.
 */

#include "CTI.h"
#include <stdio.h>              /* FILE, sprintf, printf */
#include <string.h>             /* strstr */

#ifdef STACK_DEBUG_INSTRUMENT_FUNCTIONS

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

#endif  /* STACK_DEBUG_INSTRUMENT_FUNCTIONS */


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

/* Stack_dump is called in response to errors, and shows the calling
   thread's stack */
void Stack_dump(void)
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
