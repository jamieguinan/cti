#ifndef _CTI_FILE_H_
#define _CTI_FILE_H_

#include "ArrayU8.h"
#include "String.h"

extern ArrayU8 * File_load_data(String * filename);
extern String * File_load_text(String * filename);
extern String * File_load_base64(String * filename);
extern String_list *Files_glob(String *path, String *pattern);
extern int File_exists(String *path);
extern int File_load_int(String * path, int * result);
extern int File_make_dir(String * path);
extern int File_copy(String * oldpath, String * newpath);

#endif
