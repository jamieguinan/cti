#include "CTI.h"
#include "Mem.h"
#include "String.h"
#include "Control.h"

Control_msg * Control_msg_new(String *label, Value *value)
{
  Control_msg *msg = Mem_calloc(1, sizeof(*msg));
  msg->label = label;
  msg->value = value;
  return msg;
}


void Control_msg_release(Control_msg * msg)
{
  if (msg->label) {
    String_free(&msg->label);
  }

  Value_free(msg->value);
  Mem_free(msg);
}
