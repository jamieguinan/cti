#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define streq(a,b) (strcmp((a),(b))==0)

#include "nodetree.h"

static int nodetree_debug = 0;

static void nodes_append(Node *parent, Node *child)
{
  int newcount = parent->subnodes_count + 1;
  if (parent->subnodes_available < newcount) {
    if (parent->subnodes_available == 0) {
      parent->subnodes_available = 4;
      parent->subnodes = calloc(parent->subnodes_available, sizeof(Node *));
    }
    else {
      parent->subnodes_available *= 2;
      parent->subnodes = realloc(parent->subnodes, parent->subnodes_available * sizeof(Node *));
    }
  }
  parent->subnodes[parent->subnodes_count] = child;
  parent->subnodes_count = newcount;
}


Node * node_new(const char * tag)
{
  Node *n = calloc(1, sizeof(*n));
  n->tag = strdup(tag);
  return n;
}


Node * node_add_new(Node * node, const char * tag)
{
  Node *n = node_new(tag);
  nodes_append(node, n);
  n->parent = node;
  return n;
}


void node_add(Node * node, Node *subnode)
{
  nodes_append(node, subnode);
}


void node_append_attrs(Node * node, const char *attrs)
{
  if (!node->attrs) {
    node->attrs = malloc(strlen(attrs)+2);
    sprintf(node->attrs, " %s", attrs);
  }
  else {
    node->attrs = realloc(node->attrs, strlen(node->attrs + strlen(attrs) + 2));
    strcat(node->attrs, " ");
    strcat(node->attrs, attrs);
  }
}


static void node_str_append(Node * node, String *str)
{
  int i;

  if (!node) {
    return;
  }

  if (node->tag && strlen(node->tag)) {
    String_cat1(str, s(String_sprintf("<%s", node->tag)));
    if (node->attrs) {
      String_cat1(str, s(String_sprintf("%s", node->attrs)));
    }
    if (node->subnodes_count == 1 && node->subnodes[0]->text) {
      String_cat1(str, ">");
    }
    else {
      String_cat1(str, ">\n");
    }
  }

  if (node->text) {
    String_cat1(str, s(String_sprintf("%s", node->text)));
  }

  for (i=0; i < node->subnodes_count; i++) {
    node_str_append(node->subnodes[i], str);
  }

  if (strlen(node->tag)) {
    String_cat1(str, s(String_sprintf("</%s>\n", node->tag)));
  }
}


String * node_to_string(Node * node)
{
  String *str = String_new("");
  node_str_append(node, str);
  return str;
}


void node_fwrite(Node * node, FILE *output)
{
  String *str = node_to_string(node);
  fwrite(str->bytes, 1, str->len, output);
  String_free(&str);
}


Node * Text(const char *text)
{
  Node *n = node_new("");
  n->text = strdup(text);

  return n;
}


void node_add_node_and_text(Node * node, const char * tag, const char * text)
{
  Node * x = node_add_new(node, tag);
  node_add(x, Text(text));
}


int node_content_match(Node * node, const char *value)
{
  if (node->subnodes_count && node->subnodes[0]->text && streq(node->subnodes[0]->text, value)) {
    return 1;
  }
  else {
    return 0;
  }
}

String * node_content(Node * node)
{
  if (node->subnodes_count && node->subnodes[0]->text) {
    return String_new(node->subnodes[0]->text);
  }
  else {
    return String_value_none();
  }
}


Node * node_find_subnode(Node * node, const char * subnode_tag)
{
  int i;
  if (!node) {
    return NULL;
  }
  for (i=0; i < node->subnodes_count; i++) {
    Node * subnode = node->subnodes[i];
    if (streq(subnode->tag, subnode_tag)) {
      return subnode;
    }
  }
  return NULL;
}


static Node * nfabp2(Node * node, String_list * components, int ci, const char *value, const char *adjacent_label)
{
  /* Recursive helper for node_find_adjacent_by_path() */
  int i;
  int leaf_check;
  const char * component_tag = s(String_list_get(components, ci));
  if (ci == String_list_len(components)-1) {
    leaf_check = 1;
  }
  else {
    leaf_check = 0;
  }

  for (i=0; i < node->subnodes_count; i++) {
    Node * testnode = node->subnodes[i];
    if (streq(testnode->tag, component_tag)) {
      if (leaf_check) {
        if (node_content_match(testnode, value)) {
          /* Found the target value, now check parent tree for desired adjacent node.  */
          return node_find_subnode(node, adjacent_label);
        }
      }
      else {
        Node * x = nfabp2(testnode, components, ci+1, value, adjacent_label);
        if (x) return x;
      }
    }
  }
  return NULL;
}


Node * node_find_adjacent_by_path(Node * node, const char * path, const char *value, const char *adjacent_label)
{
  /* This is intended to find a node where the path+value uniquely defines a
   * subtree, but you want a node adjacent to the final element in the
   * path.  */
  if (!node || !path) {
    return NULL;
  }

  String_list * components = String_split_s(path, "/");
  if (String_list_is_none(components)) {
    return NULL;
  }

  return nfabp2(node, components, 0, value, adjacent_label);
}


Node * node_find_subnode_by_path_match(Node * node, const char * path, const char *subnode_text)
{
  int i;
  if (!node) {
    return NULL;
  }

  if (nodetree_debug) printf("\nnode=%p (%s) path=%s\n"
         , node
         , node->tag ? node->tag : ""
         , path ? path:"(empty)"
         );

  if (!path) {
    /* End of path search.  If there is a text subnode, return it,
       first comparing to subnode_text if supplied. */
    if (node->subnodes_count
        && node->subnodes[0]->text) {
      if (subnode_text) {
        /* Require a match, or return NULL. */
        if (streq(node->subnodes[0]->text, subnode_text)) {
          return node;
        }
        else {
          return NULL;
        }
      }
      else {
        return node;
      }
    }
    else {
      puts("");
      return NULL;
    }
  }

  char local_path[strlen(path)+1]; strcpy(local_path, path);
  char *slash = strchr(local_path, '/');

  if (nodetree_debug) printf("slash=%p\n", slash);

  if (slash) {
    *slash = 0;                 /* Split string by replacing slash with zero. */
  }

  if (nodetree_debug) printf("local_path=%s\n", local_path);

  for (i=0; i < node->subnodes_count; i++) {
    Node * subnode = node->subnodes[i];
    //printf("i=%d subnode->tag=%p (%s)\n", i, subnode->tag, subnode->tag ? subnode->tag :"");
    if (subnode->tag && streq(subnode->tag, local_path)) {
      /* Try again with remainder of path. */
      Node *x = node_find_subnode_by_path_match(subnode, slash ? slash+1 : NULL, subnode_text);
      if (x) {
        return x;
      }
      else {
        continue;
      }
    }
  }

  /* Path component not found at current level! */
  return NULL;
}


Node * node_find_subnode_by_path(Node * node, const char * path)
{
  return node_find_subnode_by_path_match(node, path, NULL);
}
