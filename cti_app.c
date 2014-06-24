#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <unistd.h>		/* sleep */

#include "CTI.h"

Callback *ui_callback;

int app_code(int argc, char *argv[])
{
  Config_buffer *cb;
  int argn = 1;
  const char *input_arg = NULL;

  // Template_list(0);

  /* Set up the callback before starting anything else. */
  ui_callback = Callback_new();

  /* Set up a scripting handler, and set up initial input. */
  Instance *s = Instantiate("ScriptV00");

  while (argn < argc) {
    char temp_arg[strlen(argv[argn])+1];
    strcpy(temp_arg, argv[argn]);
    char *eq = strchr(temp_arg, '=');
    if (eq) {
      *eq = 0;
      CTI_cmdline_add(temp_arg, eq+1);
    }
    else {
      input_arg = argv[argn];
    }
    argn += 1;
  }

  if (input_arg) {
    cb = Config_buffer_new("input", argv[1]);
    PostData(cb,  &s->inputs[0]);
  }
  else {
    /* Use "" to indicate stdin */
    cb = Config_buffer_new("input", "");
    PostData(cb,  &s->inputs[0]);
  }

  while (1) {
    /* Certain platforms (like OSX) require that UI code be called
       from the main application thread, so wait here until the
       ui_main function pointer is filled in, then call it.  It can't
       hurt to do the same on platforms without that requirement, so
       do the same always. */
    if (ui_main) {
      ui_main(argc, argv);
    }
    else {
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);
    }
  }

  return 0;
}
