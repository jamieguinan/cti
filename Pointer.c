#include "CTI.h"
#include "Pointer.h"

Pointer_event *Pointer_event_new(int x, int y, uint8_t button, uint8_t state)
{
  Pointer_event *p = Mem_calloc(1, sizeof(*p));
  p->x = x;
  p->y = y;
  p->button = button;
  p->state = state;
  return p;
}
