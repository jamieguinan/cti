#ifndef _CFG_H_
#define _CFG_H_

typedef struct {
  int verbosity;
  int logging;
  int mem_tracking;
} Cfg;

extern Cfg cfg;

#endif
