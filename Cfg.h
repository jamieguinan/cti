#ifndef _CFG_H_
#define _CFG_H_

typedef struct {
  int verbosity;
  int logging;
  int pause;
  int max_pending_messages;
} Cfg;

extern Cfg cfg;

#endif
