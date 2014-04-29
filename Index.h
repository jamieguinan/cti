#ifndef _CTI_INDEX_H_
#define _CTI_INDEX_H_

/* Key/value index.  Keys of various kinds are allowed, values are
 * always "void *".  Values are treated as opaque, never accessed or
 * freed. 
 */

#include "String.h"

struct _index_node;		/* Incomplete type, defined internally. */

typedef struct {
  struct _index_node * _nodes;			/* internal storage for key/value pairs */
} Index;

enum { 
  INDEX_NO_ERROR,
  INDEX_DUPLICATE_KEY,
  INDEX_KEY_NOT_FOUND,
  INDEX_NO_KEY,
  INDEX_NO_IDX,
};

/* New/experimental API. */
extern Index * Index_new(void);
extern void Index_clear(Index **_idx);

extern void Index_add_string(Index *idx, String * stringKey, void *item, int * err);
extern void * Index_find_string(Index *idx, String * stringKey, int * err);
extern void Index_del_string(Index *idx, String * stringKey, int * err);
extern void * Index_pull_string(Index *idx, String * stringKey, int * err);

extern void Index_add_ptrkey(Index *idx, void * voidKey, void *item, int * err);
extern void * Index_find_ptrkey(Index *idx, void * voidKey, int * err);
extern void Index_del_ptrkey(Index *idx, void * voidKey, int * err);
extern void * Index_pull_ptrkey(Index *idx, void * voidKey, int * err);

#endif
