/* 
 * Index module.  Calculates a numeric key based on string key.
 * Uses a binary tree for storing and searching,
 * 
 */

#include "Index.h"
#include "Mem.h"
#include "String.h"

#include <string.h> /* memset */

typedef struct _hash_node {
  struct _hash_node *left;
  struct _hash_node *right;
  String *keyStr;
  void *value;
  uint32_t key;			/* numeric key for searching */
  int count;			/* if count > 1, verify and maybe go down left */
} Index_node;

static Index_node *Index_node_new(uint32_t key, String *keyStr, void *value)
{
  Index_node *node = Mem_malloc(sizeof(*node));
  *node = (Index_node) {
    .left = 0L,
    .right = 0L,
    .keyStr = keyStr,		/* NOTE: no reference taken! */
    .value = value,
    .key = key,
    .count = 1,
  };
  return node;
}

Index *Index_new(void)
{
  Index *idx = Mem_calloc(1, sizeof(*idx));
  return idx;
}

#include <stdio.h>
#include <unistd.h>

uint32_t key_from_string(String *keyStr)
{
  /* Generate a 32-bit hash key from string.  Fast is good.  Wide
     distribution over key space is good.  Want each string bit
     to perturb all key bits. */
  uint32_t key = 0x90a4ae8c;	/* Random number from a Python session the day I wrote this. */
  char *p = keyStr->bytes;
  while (*p) {
    uint8_t b = *p;
    uint8_t rotate = b & 0x1f;		/* Rotate up to 5 bits. */
    key = (key << rotate) | (key >> (32 - rotate));
    key ^= ((b << 23) | (b << 14) | (b << 6) | (b << 0));
    // key ^= ((b << 24) | (b << 16) | (b << 8) | (b << 0));
    p++;
  }
  return key;
}    


void Index_update(Index **_idx, String *keyStr, void *item)
{
  Index *idx = *_idx;

  if (!idx) {
    idx = *_idx = Index_new();
  }

  Index_node *node = (Index_node *)idx->_nodes;
  Index_node *parent = 0L;
  uint32_t key;

  key = key_from_string(keyStr);

  while (node) {
    parent = node;
    if (key < node->key) {
      node = node->left;
      if (!node) {
	parent->left = Index_node_new(key, keyStr, item);
      }
    }
    else if (key > node->key) {
      node = node->right;
      if (!node) {
	parent->right =  Index_node_new(key, keyStr, item);
      }
    }
    else if (key == node->key) {
      /* In case of key collision, just push down the left side. */
      idx->collisions += 1;
      node->count += 1;
      node = node->left;
      if (!node) {
	parent->left =  Index_node_new(key, keyStr, item);
      }
    }
  }

  if (!parent) {
    /* Create top node in tree. */
    idx->_nodes = Index_node_new(key, keyStr, item);    
  }
}

void *Index_find(Index *idx, String *keyStr, int * found)
{
  Index_node *p = (Index_node *)idx->_nodes;
  uint32_t key;
  void *value = 0L;

  key = key_from_string(keyStr);

  *found = 0;

  while (p) {
    if (key < p->key) {
      p = p->left;
    }
    else if (key > p->key) {
      p = p->right;
    }
    else if (key == p->key) {
      if (p->count == 1) {
	/* Only one instance of this numeric key, just return the value. */
	value = p->value;
	*found = 1;
	break;
      }
      else if (String_cmp(keyStr, p->keyStr) == 0) {
	/* More than one instance of this numeric key, and string key matches. */
	value = p->value;
	*found = 1;
	break;
      }
      else {
	/* Keep searching down left tree... */
	p = p->left;
      }
    }
  }

  /* It is possible that value will be 0L, which means key was not found! 
     FIXME: But what if the value WAS legitimately 0?  Maybe need to pass
     a "int * found" parameter. */
  return value;
}

static void Index_analyze_1(Index_node *p, int depth, int *maxDepth,
			   int *leftNodes, int *rightNodes) 
{
  if (depth > *maxDepth) {
    *maxDepth = depth;
  }

  if (p->left) {
    *leftNodes += 1;
    Index_analyze_1(p->left, depth+1, maxDepth, leftNodes, rightNodes);
  }

  if (p->right) {
    *rightNodes += 1;
    Index_analyze_1(p->right, depth+1, maxDepth, leftNodes, rightNodes);
  }
}

void Index_analyze(Index *idx)
{
  Index_node *p = (Index_node *)idx->_nodes;
  int maxDepth = 0;
  int leftNodes = 0;
  int rightNodes = 0;

  if (p) {
    Index_analyze_1(p, 0, &maxDepth, &leftNodes, &rightNodes);
  }

  printf("max depth=%d, %d left nodes, %d right nodes, %d collisions\n", 
	 maxDepth, leftNodes, rightNodes, idx->collisions);
}

static void clear_nodes(Index_node *p)
{
  if (p->left) {
    clear_nodes(p->left);
  }
  if (p->right) {
    clear_nodes(p->right);
  }
  if (p->keyStr) {
    String_free(&p->keyStr);
  }
  memset(p, 0, sizeof(*p));
  Mem_free(p);
}

void Index_clear(Index **_idx)
{
  Index *idx = *_idx;
  Index_node *p = idx->_nodes;
  clear_nodes(p);
  Mem_free(idx);
  *_idx = NULL;
}
