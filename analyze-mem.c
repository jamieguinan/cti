/*
 * This program analyzes text generated from StackDebug.c.
 *
 * Compile command,
 
  gcc analyize-mem.c Index.c Array.c Cfg.c StackDebug.c Mem.c CTI.c locks.c dpf.c String.c -lpthread -o analyize-mem

  *
  */

#include <stdio.h>
#include <inttypes.h>

#include "CTI.h"
#include "StackDebug.h"

int leftovers = 0;
InstanceGroup *gig;

Index * symbol_map = NULL;

void bug_callback(Index_node *node)
{
  printf("0x%" PRIu32 "x %s %s\n", node->key, s(node->stringKey), s((String *)node->value));
}

void post_callback(Index_node *node)
{
  /* /0x4324c9/0x408ff7/0x408db6/0x409b32/0x42a40f */

  String * stackpath = node->value;

  if (symbol_map) {
    String_list * parts = String_split(stackpath, "/");
    int i;
    for (i=1; i < String_list_len(parts)-1; i++) {
      int err;
      String * addrstr = String_list_get(parts, i);
      String * symstr = Index_find_string(symbol_map,
					  addrstr,
					  &err);
      if (i > 1) {
	printf("/"); 
      }
      printf("%s", s(symstr));
    }
    printf("\n"); 
  }
  else {
    printf("leftover: %s\n", s(stackpath));
  }
  leftovers += 1;
}

static void load_map(const char *mapfile)
{
  FILE *f = fopen(mapfile, "r");
  if (!f) {
    perror(mapfile);
    return;
  }

  symbol_map = Index_new();

  while (1) {
    char line[256];
    if (fgets(line, sizeof(line), f) == NULL) {
      break;
    }
    String_list * parts = String_split_s(line, " ");
    if (String_list_len(parts) != 3) {
      String_list_free(&parts);
      continue;
    }
    
    String * addrstr = String_list_get(parts, 0);
    String * typestr = String_list_get(parts, 1);
    String * symstr = String_list_get(parts, 2);
    String_trim_right(symstr);

    if (String_eq(typestr, S("t")) || String_eq(typestr, S("T"))) {
      uint64_t addr;
      sscanf(s(addrstr), "%lx", &addr);
      //printf("addr=0x%lx\n", addr);
      String * addrstr_shortened = String_sprintf("0x%lx", addr);
      int err;
      Index_add_string(symbol_map, 
		       addrstr_shortened,
		       String_dup(symstr),
		       &err);
    }

    String_list_free(&parts);
  }
  fclose(f);
}


int main(int argc, char *argv[])
{
  gig = InstanceGroup_new();

  FILE *f;
  if (argc < 2) {
    printf("Usage: %s debugtextfile [mapfile]\n", argv[0]);
    return 1;
  }
  if (argc >= 2) {
    f = fopen(argv[1], "r");
    if (!f) {
      return 1;
    }
  }

  if (argc ==3) {
    load_map(argv[2]);
  }


  char buffer[256];
  
  Index * idx = Index_new();
  int allocations = 0;
  int frees = 0;

  while (1) {
    if (fgets(buffer, sizeof(buffer), f) == NULL) {
      break;
    }
    String_list * parts = String_split_s(buffer, "::");
    if (String_list_len(parts) != 4) {
      continue;
    }
    String * stackpath = String_list_get(parts, 1);
    String * addr = String_list_get(parts, 2);
    String * op = String_list_get(parts, 3);
    String_trim_right(op);

    // printf("%s %s %s\n", s(addr), s(stackpath), s(op));

    int err;
    if (String_eq(op, S("+"))) {
      Index_add_string(idx, 	/* idx */
		       addr, 	/* stringKey */
		       String_dup(stackpath), /* value */
		       &err);
      allocations += 1;
      // Index_analyze(idx);
      // printf("allocations - frees = %d - %d = %d\n", allocations, frees, allocations - frees);
      if (err) {
	{ printf("[%s] + err = %d\n", s(addr), err); }
	Index_walk(idx, post_callback);
	return 1;
      }
    }
    else if (String_eq(op, S("-"))) {
      Index_pull_string(idx,
		       addr,
		       &err);
      frees += 1;
      // Index_analyze(idx);
      // printf("allocations - frees = %d - %d = %d\n", allocations, frees, allocations - frees);
      if (err) {
	{ printf("[%s] - err = %d\n", s(addr), err); }
	//Index_walk(idx, post_callback);
	//return 1;
      }
    }
    String_list_free(&parts);
  }


  fclose(f);

  Index_walk(idx, post_callback);
  Index_analyze(idx);

  printf("%d allocations\n", allocations);
  printf("%d frees\n", frees);
  printf("%d calculated leftovers\n", (allocations - frees));
  printf("%d leftovers\n", leftovers);

  
  return 0;
}
