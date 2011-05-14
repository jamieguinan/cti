#include "Template.h"
#include "Keycodes.h"

Keycode_message * Keycode_message_new(int keycode)
{
  Keycode_message * msg = Mem_malloc(sizeof(*msg));
  msg->keycode = keycode;
  return msg;
}


void Keycode_message_cleanup(Keycode_message **msg)
{
  Mem_free(*msg);
  *msg = 0L;
}
