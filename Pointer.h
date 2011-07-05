#ifndef _POINTER_H_
#define _POINTER_H_

typedef struct {
  int x;
  int y;
  uint8_t button;
  uint8_t state;
} Pointer_event;

Pointer_event *Pointer_event_new(int x, int y, uint8_t button, uint8_t state);

#endif
