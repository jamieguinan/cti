#ifndef _XMIXEDREPLACE_H_
#define _XMIXEDREPLACE_H_

#error Try using Source/Sink instead of this module...

#include <stdint.h>
#include <sys/time.h>

/* XMixedReplace buffer */
typedef struct {
  uint8_t *data;
  int data_length;
  struct timeval tv;
} XMixedReplace_buffer;

extern XMixedReplace_buffer *XMixedReplace_buffer_new(void *data, int data_length, const char *mime_type);
extern void XMixedReplace_buffer_discard(XMixedReplace_buffer *buffer);

#endif
