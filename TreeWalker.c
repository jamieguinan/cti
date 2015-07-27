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

//#include "CTI.h"
#include "TreeWalker.h"
#include "String.h"

void TreeWalker_walk(String *dstr, int (*callback)(String * path, unsigned char dtype) )
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
      int ignore = 0;
      if (callback) { ignore = callback(nextdir, DT_DIR); }
      if (ignore == 0) { 
	TreeWalker_walk(nextdir, callback);
      }
      String_free(&nextdir);
    }
    else if (d_type == DT_REG) {
      String *fullname = String_sprintf("%s/%s", s(dstr), de->d_name);
      if (callback) {  callback(fullname, DT_REG); }
      String_free(&fullname);
    }
  }

  closedir(d);
}


#ifdef TEST

int xcallback(String *path, unsigned char dtype)
{
  if (dtype == DT_DIR) {
    printf("   directory: %s\n", s(path));
  }
  else if (dtype == DT_REG) {
    printf("regular file: %s\n", s(path));
  }
  return 0;
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
