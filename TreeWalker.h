#ifndef _TREEWALKER_H_
#define _TREEWALKER_H_

#include "String.h"

// extern void TreeWalker_init(void);
extern int TreeWalker_walk(String *dstr,
                           int (*callback)(String * path, unsigned char dtype) );

#endif
