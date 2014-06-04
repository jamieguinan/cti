#ifndef _CTI_CONTROL_H_
#define _CTI_CONTROL_H_

typedef struct {
  String *label;
  Value *value;
} Control_msg;

extern Control_msg * Control_msg_new(String *label, Value *value);
extern void Control_msg_discard(Control_msg * msg);

#endif
