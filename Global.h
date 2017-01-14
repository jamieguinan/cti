#ifndef _CTI_GLOBAL_H
#define _CTI_GLOBAL_H

typedef struct {
  struct {
    int width;
    int height;
  } display;
} Global_t;

extern Global_t global;

extern void Global_init();

#endif
