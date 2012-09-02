#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"

Callback *ui_callback;

int app_code(int argc, char *argv[])
{
  Config_buffer *cb;

  // Template_list();

  /* Set up the callback before starting anything else. */
  ui_callback = Callback_new();

  /* Set up a scripting handler, and set up initial input. */
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

  while (1) {
    /* Certain platforms (like OSX) require that UI code be called from the 
       main application thread.  So the code waits here for a message containing
       a UI function, and calls it. */
    Callback_wait(ui_callback);
    if (ui_callback->func) {
      ui_callback->func(ui_callback->data);
    }
  }

  return 0;
}
