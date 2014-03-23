#include "CTI.h"

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

