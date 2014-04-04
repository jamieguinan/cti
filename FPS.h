#ifndef _FPS_H_
#define _FPS_H_

#include <sys/time.h>		/* struct timeval */

/* FPS */
typedef struct {
  double timestamp_last;
  float period;
  int count;
} FPS;
extern void FPS_show(FPS *fps);

#endif
