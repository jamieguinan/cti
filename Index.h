#ifndef _CTI_INDEX_H_
#define _CTI_INDEX_H_

#include "String.h"

typedef struct {
  // long _l1[2];			/* last key/value looked up */
  // long _l2[8];			/* last 4 key/value looked up */
  void *_nodes;			/* internal tree of key/value pairs */
  long len;
  long _available;
  int collisions;
} Index;

extern void Index_update(Index **idx_, String *keyStr, void *item);
extern void * Index_find(Index *idx_, String *keyStr, int *found);
extern void Index_clear(Index **_idx);

#endif
