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
#include <errno.h>              /* errno */

//#include "CTI.h"
#include "TreeWalker.h"
#include "String.h"
#include "localptr.h"

int TreeWalker_walk(String *dstr,
                    int (*callback)(String * path, unsigned char dtype, void * cbdata),
                    void * cbdata)
{
  DIR * d = opendir(s(dstr));
  if (!d) {
    perror(s(dstr));
    return errno;
  }

  int ret = 0;

  struct dirent * de;

  while (1) {
    errno = 0;
    de = readdir(d);
    if (de == NULL) {
      /* Note, could be end of directory or error. */
      ret = errno;
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
	else {
	  d_type = DT_UNKNOWN;
	}
      }
    }

    if (d_type == DT_DIR) {
      if (streq(de->d_name, ".")
	  || streq(de->d_name, "..")) {
	continue;
      }
      localptr(String, nextdir) = String_sprintf("%s/%s", s(dstr), de->d_name);
      int ignore = 0;
      if (callback) { ignore = callback(nextdir, DT_DIR, cbdata); }
      if (ignore == 0) { 
	ret = TreeWalker_walk(nextdir, callback, cbdata);
      }
    }
    else if (d_type == DT_REG || d_type == DT_LNK) {
      localptr(String, fullname) = String_sprintf("%s/%s", s(dstr), de->d_name);
      if (callback) {
        ret = callback(fullname, d_type, cbdata);
      }
    }
    else {
      fprintf(stderr, "skipping entry with d_type=%d\n", d_type);
    }

    if (ret) {
      break;
    }
  }

  closedir(d);

  return ret;
}


#ifdef TEST

int xcallback(String *path, unsigned char dtype, void *)
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
  TreeWalker_walk(S("/home/guinan/.maildir"), xcallback, NULL);
  return 0;
}
#endif
