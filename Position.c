#include "CTI.h"
#include "Position.h"

Position_message *Position_message_new(void)
{
  Position_message *p = Mem_calloc(1, sizeof(*p));
  return p;
}


void Position_message_release(Position_message *p)
{
  Mem_free(p);
}
