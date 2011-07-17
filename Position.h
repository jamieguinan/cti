#ifndef _POSITION_H_
#define _POSITION_H_

typedef struct {
  float x, y, z;
  float rx, ry, rz;
} Position_message;

Position_message *Position_message_new(void);
void Position_message_discard(Position_message *pm);


#endif
