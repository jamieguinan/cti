#ifndef _NODETREE_H_
#define _NODETREE_H_

#include <stdio.h>
#include "String.h"

/* Generic "Node" code, written to support HTML and XML, but could
   be used for anything where it fits. */

typedef struct _Node {
  const char *tag;
  struct _Node **subnodes;
  char *attrs;
  int subnodes_count;
  int subnodes_available;

  char *text;

  struct _Node *parent;		/* optional back-pointer */
} Node;

extern Node * node_new(const char * tag);
extern Node * node_add_new(Node * node, const char * tag);
extern void node_append_attrs(Node * node, const char * attrs);
extern void node_add(Node * node, Node * subnode);
extern void node_flush(Node ** node, FILE * output);
extern void node_fwrite(Node * node, FILE * output);
extern Node * Text(const char * text);
extern void node_add_node_and_text(Node * node, const char * tag, const char * text); /* convenience function */
extern Node * node_find_subnode(Node * node, const char * child_tag);
extern Node * node_find_subnode_by_path(Node * node, const char * path);
extern Node * node_find_subnode_by_path_match(Node * node, const char * path, const char * text);
extern Node * node_none(Node * node); /* Use this instead of checking for NULL result. */
extern String * node_to_string(Node * node);
extern int node_content_match(Node * node, const char *value);
extern String * node_content(Node * node);

#endif
