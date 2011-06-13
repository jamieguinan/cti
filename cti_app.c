#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"


int app_code(int argc, char *argv[])
{
  int rc = 0;
  Config_buffer *cb;

  // Template_list();

  Instance *s = Instantiate("ScriptV00");

  if (argc == 2) {
    cb = Config_buffer_new("input", argv[1]);
    PostData(cb,  &s->inputs[0]);
  }
  else if (argc == 1) {
    /* "" means use stdin */
    cb = Config_buffer_new("input", "");
    PostData(cb,  &s->inputs[0]);
  }

  if (rc != 0) {
    return 1;
  }

  while (1) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);
  }

  return 0;
}
