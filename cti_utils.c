#include "cti_utils.h"
#include <time.h>

void cti_msleep(unsigned long msecs)
{
  struct timespec ts;
  ts.tv_sec = msecs / 1000;
  ts.tv_nsec = (msecs % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void cti_usleep(unsigned long usecs)
{
  struct timespec ts;
  ts.tv_sec = usecs / 1000000;
  ts.tv_nsec = (usecs % 1000000) * 1000;
  nanosleep(&ts, NULL);
}
