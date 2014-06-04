/* 
 * Index module, see Index.h for description.
 * 
 * This is 2nd year Computer Science stuff, but as a professional
 * programmer I've been spoiled by Python, C++ STL, and other
 * containerss, so I haven't written code like this since ~1992.
 * Writing this evokes memories of my compsci professors drawing nodes
 * on the whiteboard...
 */

#include "Index.h"
#include "Mem.h"
#include "String.h"

#include <string.h>		/* memset */
#include <inttypes.h>		/* PRIu32 */

static Index_node * Index_node_new(uint32_t key, String *stringKey, void * voidKey, void *value)
{
  Index_node *node = Mem_malloc(sizeof(*node));
  *node = (Index_node) {
    .left = 0L,
    .right = 0L,
    .stringKey = String_dup(stringKey),
    .voidKey = voidKey,
    .value = value,
    .key = key,
    .count = 1,
  };
  return node;
}

static void Index_node_free(Index_node ** ppnode)
{
  Index_node * node = *ppnode;
  if (node->stringKey) {
    String_free(&(node->stringKey));
  }
  Mem_free(node);
  *ppnode = 0;
}


Index *Index_new(void)
{
  Index *idx = Mem_calloc(1, sizeof(*idx));
  return idx;
}

#include <stdio.h>
#include <unistd.h>

static uint32_t key_from_bytes(char * bytes, int len)
{
  /* Generate a 32-bit hash key from a set of bytes.  Fast is good.
     Wide distribution over key space is good.  Want each string bit
     to perturb all key bits. */
  uint32_t key = 0x90a4ae8c;	/* Random number from a Python session the day I wrote this. */
  char *p = bytes;
  int i;
  //printf("%d bytes[", len);
  for (i=0; i < len; i++) {
    uint8_t b = p[i];
    //printf(" %02x", b);
    uint8_t rotate = b & 0x1f;		/* Rotate up to 5 bits. */
    key = (key << rotate) | (key >> (32 - rotate));
    key ^= ((b << 23) | (b << 14) | (b << 6) | (b << 0));
    /* key ^= ((b << 24) | (b << 16) | (b << 8) | (b << 0)); */
  }
  //printf(" ], key=%" PRIu32 "\n", key);
  return key;
}


uint32_t key_from_string(String *stringKey)
{
  //printf("stringKey=%s, ", s(stringKey));
  return key_from_bytes(s(stringKey), String_len(stringKey));
}


uint32_t key_from_ptr(void * voidKey)
{
  return key_from_bytes((char*)voidKey, sizeof(voidKey));
}


static void Index_analyze_2(Index_node *p, int depth, int *maxDepth,
			   int *leftNodes, int *rightNodes) 
{
  if (depth > *maxDepth) {
    *maxDepth = depth;
  }

  if (p->left) {
    *leftNodes += 1;
    Index_analyze_2(p->left, depth+1, maxDepth, leftNodes, rightNodes);
  }

  if (p->right) {
    *rightNodes += 1;
    Index_analyze_2(p->right, depth+1, maxDepth, leftNodes, rightNodes);
  }
}

void Index_analyze(Index *idx)
{
  Index_node *p = idx->_nodes;
  int maxDepth = 0;
  int leftNodes = 0;
  int rightNodes = 0;

  if (p) {
    Index_analyze_2(p, 0, &maxDepth, &leftNodes, &rightNodes);
  }

  printf("max depth=%d, %d left nodes, %d right nodes\n", 
	 maxDepth, leftNodes, rightNodes);
}


void Index_walk_2(Index_node *node, void (*callback)(Index_node *node))
{
  if (node->left) {
    Index_walk_2(node->left, callback);
  }
  if (node->right) {
    Index_walk_2(node->right, callback);
  }
  if (callback) {
    callback(node);
  }
}

void Index_walk(Index *idx, void (*callback)(Index_node *node))
{
  if (idx->_nodes) {
    Index_walk_2(idx->_nodes, callback);
  }
}

static void clear_nodes(Index_node *p)
{
  if (p->left) {
    clear_nodes(p->left);
  }
  if (p->right) {
    clear_nodes(p->right);
  }
  if (p->stringKey) {
    String_free(&p->stringKey);
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

/* 2014-Apr-29 Experiments... */
static inline void key_from_either(String * stringKey, void * voidKey, 
				   uint32_t * key,
				   int * err)
{
  *key = 0;			/* keep compiler happy */
  if (stringKey) {
    *key = key_from_string(stringKey);
    *err = INDEX_NO_ERROR;
  }
  else if (voidKey) {
    *key = key_from_ptr(voidKey);
    *err = INDEX_NO_ERROR;
  }
  else {
    fprintf(stderr, "%s: neither stringKey nor voidKey provided!\n", __func__);
    *err = INDEX_NO_KEY;
  }
}


static void insert_node(Index_node * node, uint32_t key, Index_node * new_node)
{
  /* Common insertion code. */
  while (node) {
    Index_node *parent = node;

    if (key < node->key) {
      node = node->left;
      if (!node) {
	parent->left = new_node;
      }
    }
    else if (key > node->key) {
      node = node->right;
      if (!node) {
	parent->right = new_node;
      }
    }
    else if (key == node->key) {
      /* In case of key collision, just push down the left side. */
      node = node->left;
      if (!node) {
	parent->left = new_node;
      }
    }
  }
}


static void _Index_add(Index * idx, String * stringKey, void * voidKey, void * value, int * err)
{
  uint32_t key;

  key_from_either(stringKey, voidKey, &key, err);
  if (*err == INDEX_NO_KEY) {
    return;
  }

  if (!idx) {
    fprintf(stderr, "%s: idx not set!\n", __func__);
    *err = INDEX_NO_IDX;
    return;
  }

  Index_node * new_node = Index_node_new(key, stringKey, voidKey, value);

  Index_node *node = idx->_nodes;

  if (!node) {
    /* Create top node in tree. */
    idx->_nodes = new_node;
    goto out;
  }

  insert_node(node, key, new_node);

 out:
  *err = INDEX_NO_ERROR;
  return;
}


void Index_add_string(Index * idx, String * stringKey, void * value, int * err)
{
  _Index_add(idx, 
	      stringKey, NULL,
	      value,
	      err);
}

void Index_add_ptrkey(Index * idx, void * voidKey, void * value, int * err)
{
  _Index_add(idx, 
	     NULL, voidKey,
	     value,
	     err);
}


static void * _Index_find(Index *idx, String *stringKey, void * voidKey, int del, int * err)
{
  void *value = 0L;
  uint32_t key;

  key_from_either(stringKey, voidKey, &key, err);
  if (*err == INDEX_NO_KEY) {
    return value;
  }

  if (!idx) {
    fprintf(stderr, "%s: idx not set!\n", __func__);
    *err = INDEX_NO_IDX;
    return value;
  }

  Index_node *node = idx->_nodes;
  Index_node **parent_ptr = &(idx->_nodes);

  while (node) {
    if (key < node->key) {
      parent_ptr = &(node->left);
      node = node->left;
    }
    else if (key > node->key) {
      parent_ptr = &(node->right);
      node = node->right;
    }
    else if (key == node->key) {
      /* Duplicate hash keys are allowed, so verify that value matches. */
      if ( (stringKey && String_cmp(stringKey, node->stringKey) == 0) 
		|| (voidKey && voidKey == node->voidKey) ) {
	value = node->value;
	*err = INDEX_NO_ERROR;
	break;
      }
      else {
	/* Keep searching down left tree... */
	parent_ptr = &(node->left);
	node = node->left;
      }
    }
  }

  if (node) {
    *err = INDEX_NO_ERROR;
    if (del) {
      /* Delete this node.  4 cases to handle. */
      if (node->left && node->right) {
	/* Both subtrees.  Move left subtree under right. */
	insert_node(node->right, node->left->key, node->left);
	*parent_ptr = node->right;
      }
      else if (node->left) {
	/* Only left subtree. */
	*parent_ptr = node->left;
      }
      else if (node->right) {
	/* Only right subtree. */
	*parent_ptr = node->right;
      }
      else {
	/* No subtrees. */
	*parent_ptr = NULL;
      }
      Index_node_free(&node);
    }
  }
  else {
    *err = INDEX_KEY_NOT_FOUND;
  }

  return value;
}


void * Index_find_string(Index * idx, String * stringKey, int * err)
{
  return _Index_find(idx, 
		     stringKey, NULL,
		     0,
		     err);
}


void * Index_find_ptrkey(Index * idx, void * voidKey, int * err)
{
  return _Index_find(idx, 
		     NULL, voidKey,
		     0,
		     err);
}


void Index_del_string(Index * idx, String * stringKey, int * err)
{
  /* Find and delete, ignore return value. */
  _Index_find(idx, 
	      stringKey, NULL,
	      1,
	      err);
}


void Index_del_ptrkey(Index * idx, void * voidKey, int * err)
{
  /* Find and delete, ignore return value. */
  _Index_find(idx, 
	     NULL, voidKey,
	     1,
	     err);
}


void * Index_pull_string(Index * idx, String * stringKey, int * err)
{
  /* Find and delete, returning value. */
  return _Index_find(idx, 
		     stringKey, NULL,
		     1,
		     err);
}


void * Index_pull_ptrkey(Index * idx, void * voidKey, int * err)
{
  /* Find and delete, returning value. */
  return _Index_find(idx, 
		     NULL, voidKey,
		     1,
		     err);
}
