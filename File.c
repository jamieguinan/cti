#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "File.h"
#include "localptr.h"

ArrayU8 * File_load_data(String * filename)
{
  long len;
  long n;
  int procflag = 0;
  FILE *f = fopen(s(filename), "rb");

  if (!f) {
    // perror(s(filename));
    return 0L;
  }

  if (strncmp("/proc/", s(filename), strlen("/proc/")) == 0) {
    len = 32768;
    procflag = 1;
  }
  else {
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
  }

  ArrayU8 *a = ArrayU8_new();
  ArrayU8_extend(a, len);
  n = fread(a->data, 1, len, f);
  if (n < len && !procflag) {
    fprintf(stderr, "warning: only read %ld of %ld expected bytes from %s\n", n, len, s(filename));
  }
  a->data[n] = 0;

  fclose(f);

  return a;
}


String * File_load_text(String * filename)
{
  ArrayU8 *a = File_load_data(filename);
  if (a) {
    /* Add 1 extra byte so can add null-termination byte when converting to string. */
    ArrayU8_extend(a, a->len+1);
    a->data[a->len-1] = 0;
    return ArrayU8_to_string(&a);
  }
  else {
    return String_value_none();
  }
}

static char base64map[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String * File_load_base64(String * filename)
{
  int i;
  ArrayU8 *a = File_load_data(filename);
  if (!a) {
    return String_value_none();
  }
  
  String * result = String_new("");
  int digits[4] = {};
  char values[4];
  for (i=0; i < a->len; i++) {
    switch (i%3) {
    case 0: digits[0] = a->data[i] >> 2; digits[1] = (a->data[i] & 0x3) << 4;
      values[0] = base64map[digits[0]];
      break;
    case 1: digits[1] |= a->data[i] >> 4; digits[2] = (a->data[i] & 0xf) << 2;
      values[1] = base64map[digits[1]];
      break;
    case 2: digits[2] |= a->data[i] >> 6; digits[3] = (a->data[i] & 0x3f);
      values[2] = base64map[digits[2]];
      values[3] = base64map[digits[3]];
      String_append_bytes(result, values, 4);
      break;
    }
  }
  
  switch(i%3) {
  case 0: break;
  case 1:
    values[1] = base64map[digits[1]];
    String_append_bytes(result, values, 2);
    String_cat1(result, "==");
    break;
  case 2:
    values[2] = base64map[digits[2]];
    String_append_bytes(result, values, 3);
    String_cat1(result, "=");
    break;
  }

  return result;
}


String_list *Files_glob(String *path, String *pattern)
{
  glob_t g;
  int i;
  String_list *slst = String_list_new();
  //fprintf(stderr, "%s\n", s(path));
  String *full_pattern = String_sprintf("%s/%s", s(path), s(pattern));
  int rc = glob(s(full_pattern), 0,
		NULL,
		&g);
  if (rc == 0) {
    for (i=0; i < g.gl_pathc; i++) {
      String *tmp = String_new(g.gl_pathv[i]);
      String_list_add(slst, &tmp);
    }
    globfree(&g);
  }
  return slst;
}


int File_exists(String *path)
{
  return access(s(path), R_OK) == 0 ? 1 : 0;
}


int File_load_int(String * path, int * result)
{
  int rc = 0;
  String * contents = File_load_text(path);
  if (String_is_none(contents)) {
    fprintf(stderr, "Could not read number from %s\n", s(path));
    rc = -1;
  }
  else {
    String_trim_right(contents);
    if (sscanf(s(contents), "%d", result) != 1) {
      fprintf(stderr, "%s does not seem to contain an integer\n", s(path));
      rc = -1;
    }
  }
  String_clear(&contents);
  return rc;
}


int File_make_dir(String * path)
{
  struct stat st;
  int rc;
  rc = stat(s(path), &st);
  if (rc == 0) {
    if (S_ISDIR(st.st_mode)) {
      /* Already exists and is a directory. My decision is that for
         CTI semantics, this is not cause for warning or error. */
      return 0;
    }
    else {
      fprintf(stderr, "%s exists but is not a directory!\n", s(path));
      return -1;
    }
  }
  
  return mkdir(s(path), 0777);
}


int File_set_owner_perms(String * path, int uid, int gid, int mode)
{
  return 0;
}


int File_copy(String * oldpath, String * newpath)
{
  /* I'm not totally happy with this. I might be able to expose errors
     better if I used lower level read/write. Also, it might be nice
     if it made use of SourceSink.c instead of redudantly doing similar
     things here. */
  int ret = 0;
  
  FILE * fin = fopen(s(oldpath), "rb");
  if (!fin) {
    perror(s(oldpath));
    return 1;
  }

  FILE * fout = fopen(s(newpath), "wb");
  if (!fout) {
    perror(s(newpath));
    fclose(fin);
    return 1;
  }

  while (1) {
    uint8_t block[32000];
    int rn = fread(block, 1, sizeof(block), fin);
    // fprintf(stderr, "read %d\n", rn);
    if (rn == 0) {
      break;
    }
    int wn = fwrite(block, 1, rn, fout);
    // fprintf(stderr, "wrote %d\n", wn);    
    if (wn != rn) {
      ret = 1;
      break;
    }
  }

  /* Since stdio buffers, errors like ENOSPC (disk full) won't show up
     until a flush is attempted. */
  if (fflush(fout) != 0) {
    fprintf(stderr, "%s ", s(newpath));
    perror("fflush");
    ret = 1;
  }

  if (fclose(fout) != 0) {
    fprintf(stderr, "%s ", s(newpath));
    perror("fclose");
    ret = 1;
  }
  
  fclose(fin);
  
  return ret;
}
