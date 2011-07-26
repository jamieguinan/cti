#ifndef _CFG_H_
#define _CFG_H_

typedef struct {
  int verbosity;
  int logging;
  int mem_tracking;
  int pause;
} Cfg;

extern Cfg cfg;

#endif
