#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"

Input app_ui_input;			/* Global input, for starting UI code. */
static Instance cti_app_instance;	/* Parent structure for input. */

static void UI_handler(Instance *pi, void *msg)
{
}


int app_code(int argc, char *argv[])
{
  int rc = 0;
  Config_buffer *cb;

  // Template_list();

  /* Set up the input. */
  app_ui_input.type_label = "cti_app_ui_input";
  app_ui_input.parent = &cti_app_instance;
  app_ui_input.handler = UI_handler;

  /* Set up the app instance, so that message posting works. */
  cti_app_instance.label = "cti_app";
  cti_app_instance.inputs = &app_ui_input;
  cti_app_instance.num_inputs = 1;
  Lock_init(&cti_app_instance.inputs_lock);
  WaitLock_init(&cti_app_instance.notification_lock);

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
       a UI loop function, and calls it. */
    int (*ui_loop)(void *data);
    Handler_message *hm = GetData(&cti_app_instance, 1);
    if (hm && hm->data) {
      ui_loop = hm->data;
      ui_loop(&cti_app_instance);
    }
  }

  return 0;
}
