#ifndef _TREEWALKER_H_
#define _TREEWALKER_H_

extern void TreeWalker_init(void);

enum { TREEWALKER_DIR_ENTERING,  TREEWALKER_DIR_LEAVING };

typedef struct {
  int dtype;			/* dtype of current entry */
  /* Only relevant for directories. */
  int phase;
  unsigned int total_subdirs;
  unsigned int total_subfiles;
  unsigned long counted_subsize;
} TreeWalkerContext;

#endif
