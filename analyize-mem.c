/*
  gcc analyize-mem.c -o analyize-mem -lpthread
*/
#include "CTI.h"
#include "StackDebug.h"

#include "Index.c"
#include "Mem.c"
#include "Array.c"
#include "Cfg.c"
#include "String.c"
#include "StackDebug.c"
#include "CTI.c"
#include "locks.c"
#include "dpf.c"

int leftovers = 0;

void callback(void *value)
{
  String *xx = value;
  printf("leftovers: %s\n", s(xx));
  leftovers += 1;
}


int main(int argc, char *argv[])
{
  FILE *f;
  if (argc != 2) {
    printf("Usage: %s debugtextfile\n", argv[0]);
    return 1;
  }
  if (argc == 2) {
    f = fopen(argv[1], "r");
    if (!f) {
      return 1;
    }
  }

  char buffer[256];
  
  Index * idx = Index_new();
  int allocations = 0;
  int frees = 0;

  while (1) {
    if (fgets(buffer, sizeof(buffer), f) == NULL) {
      break;
    }
    String_list * parts = String_split_s(buffer, ",");
    String * f = String_list_get(parts, 0);
    String * x = String_list_get(parts, 1);
    String * a = String_list_get(parts, 2);
    if (!String_is_none(f) && !String_is_none(a)) {
      String_trim_right(a);
      int err;
      //printf("[%s] [%s]\n", s(f), s(a));
      if (String_eq(f, S("calloc"))) {
	Index_add_string(idx, 	/* idx */
			 a, 	/* stringKey */
			 String_dup(x), /* value */
			 &err);
	allocations += 1;
      }
      else if (String_eq(f, S("malloc"))) {
	Index_add_string(idx, 	/* idx */
			 a, 	/* stringKey */
			 String_dup(x), /* value */
			 &err);
	allocations += 1;
      }
      else if (String_eq(f, S("realloc"))) {
	Index_add_string(idx, 	/* idx */
			 a, 	/* stringKey */
			 String_dup(x), /* value */
			 &err);
	allocations += 1;
      }
      else if (String_eq(f, S("free"))) {
	String * xx = Index_pull_string(idx, 	/* idx */
					a, 	/* stringKey */
					&err);
	frees += 1;
      }
      else if (String_eq(f, S("rfree"))) {
	String * xx = Index_pull_string(idx, 	/* idx */
					a, 	/* stringKey */
					&err);
	frees += 1;
      }

    }
    String_list_free(&parts);
  }

  fclose(f);

  Index_analyze(idx);
  Index_walk(idx, callback);

  printf("%d allocations\n", allocations);
  printf("%d frees\n", frees);
  printf("%d calculated leftovers\n", (allocations - frees));
  printf("%d leftovers\n", leftovers);

  
  return 0;
}
