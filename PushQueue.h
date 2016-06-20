#ifndef _PUSH_QUEUE_H_
#define _PUSH_QUEUE_H_

extern void PushQueue_init(void);

typedef struct {
  String * local_path;
  String * file_to_send;
  String * file_to_delete;
} PushQueue_message;

extern PushQueue_message *  PushQueue_message_new(void);
extern void PushQueue_message_free(PushQueue_message **);

#endif
