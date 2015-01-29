#ifndef _CTI_FILE_H_
#define _CTI_FILE_H_

#include "ArrayU8.h"
#include "String.h"

extern ArrayU8 * File_load_data(String * filename);

extern String * File_load_text(String * filename);

extern String_list *Files_glob(String *path, String *pattern);

extern int File_exists(String *path);


#endif
