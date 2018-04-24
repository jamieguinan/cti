#ifndef _CTI_INDEX_H_
#define _CTI_INDEX_H_

/* Key/value index with binary tree implementation.  Keys of various
 * kinds are allowed, values are always "void *".  Values are treated
 * as opaque, caller must free.
 */
#include <stdint.h>		/* uint32_t */

#include "String.h"

/* I had originally made _index_node an incomplete type, which is legal
 * in C as long as one only uses pointers, but I decided it was worth
 * it to expose the internals.
 */
typedef struct _index_node {
  struct _index_node * left;
  struct _index_node * right;

  /* Key types. */
  String * stringKey;
  void * voidKey;

  uint32_t key;			/* hash-generated numeric key for searching */

  void * value;
} Index_node;


typedef struct {
  struct _index_node * _nodes;			/* storage for key/value pairs */
  int count;
} Index;

enum { 
  INDEX_NO_ERROR,
  INDEX_DUPLICATE_KEY,
  INDEX_KEY_NOT_FOUND,
  INDEX_NO_KEY,
  INDEX_NO_IDX,
  INDEX_NO_DEST,
};

/* Flags. */
#define INDEX_ADD_OVERWRITE (1<<0)

/* New/experimental API. */
extern Index * Index_new(void);
extern void Index_clear(Index **_idx);
extern void Index_analyze(Index *idx);
extern void Index_walk(Index *idx, void (*callback)(Index_node *node));

extern void Index_add_string(Index *idx, String * stringKey, void *item, int * err);
extern void Index_replace_string(Index *idx, String * stringKey, void *item, void **old_item, int * err);
extern void * Index_find_string(Index *idx, String * stringKey, int * err);
extern void Index_del_string(Index *idx, String * stringKey, int * err);
extern void * Index_pull_string(Index *idx, String * stringKey, int * err);

extern void Index_add_ptrkey(Index *idx, void * voidKey, void *item, int * err);
extern void Index_replace_ptrkey(Index *idx, void * voidKey, void *item, void **old_item, int * err);
extern void * Index_find_ptrkey(Index *idx, void * voidKey, int * err);
extern void Index_del_ptrkey(Index *idx, void * voidKey, int * err);
extern void * Index_pull_ptrkey(Index *idx, void * voidKey, int * err);

#endif
