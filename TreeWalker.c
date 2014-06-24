/*
 * Tree walker module.
 * COMPILE: gcc -DTEST TreeWalker.c String.c Mem.c -o TreeWalker_test
 * RUN: ./TreeWalker_test
 */


#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <sys/types.h>		/* opendir, stat */
#include <dirent.h>		/* opendir, readdir */
#include <sys/stat.h>		/* stat */
#include <unistd.h>		/* stat */

#include "CTI.h"
#include "TreeWalker.h"
#include "String.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input TreeWalker_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output TreeWalker_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} TreeWalker_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  // Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void TreeWalker_instance_init(Instance *pi)
{
  TreeWalker_private *priv = (TreeWalker_private *)pi;
}


static Template TreeWalker_template = {
  .label = "TreeWalker",
  .priv_size = sizeof(TreeWalker_private),  
  .inputs = TreeWalker_inputs,
  .num_inputs = table_size(TreeWalker_inputs),
  .outputs = TreeWalker_outputs,
  .num_outputs = table_size(TreeWalker_outputs),
  .tick = NULL,
  .instance_init = TreeWalker_instance_init,
};

void TreeWalker_init(void)
{
  // Template_register(&TreeWalker_template);
}

TreeWalker_walk(String *dstr, void (*callback)(String * path, unsigned char dtype) )
{
  DIR * d = opendir(s(dstr));
  if (!d) {
    perror(s(dstr));
    return;
  }

  struct dirent * de;

  while (1) {
    de = readdir(d);
    if (de == NULL) {
      /* Note, could be end of directory or error. */
      break;
    }

    unsigned char d_type = de->d_type;

    if (d_type == 0) {
      /* Fallback, use stat(). */
      char path[strlen(s(dstr)) + strlen(de->d_name) + 1];
      sprintf(path, "%s/%s", s(dstr), de->d_name);
      struct stat st;
      if (lstat(path, &st) == 0) {
	if (S_ISLNK(st.st_mode)) {
	  d_type = DT_LNK;
	}
	else if (S_ISREG(st.st_mode)) {
	  d_type = DT_REG;
	}
	else if (S_ISDIR(st.st_mode)) {
	  d_type = DT_DIR;
	}
      }
    }

    if (d_type == DT_DIR) {
      if (streq(de->d_name, ".")
	  || streq(de->d_name, "..")) {
	continue;
      }
      String *nextdir = String_sprintf("%s/%s", s(dstr), de->d_name);
      if (callback) { callback(dstr, DT_DIR); }
      TreeWalker_walk(nextdir, callback);
      String_free(&nextdir);
    }
    else if (d_type == DT_REG) {
      String *fullname = String_sprintf("%s/%s", s(dstr), de->d_name);
      if (callback) {  callback(dstr, DT_REG); }
      String_free(&fullname);
    }
  }

  closedir(d);
}


#ifdef TEST

void xcallback(String *path, unsigned char dtype)
{
  if (dtype == DT_DIR) {
    printf("   directory: %s\n", s(path));
  }
  else if (dtype == DT_REG) {
    printf("regular file: %s\n", s(path));
  }
}

int main(int argc, char *argv[])
{
  //TreeWalker_walk(S("/home/xplane9"));
  //TreeWalker_walk(S("/usr/local/kernels"));
  //TreeWalker_walk(S("/storage00/usr/local/kernels"));
  TreeWalker_walk(S("/home/guinan/.maildir"), xcallback);
  return 0;
}
#endif
