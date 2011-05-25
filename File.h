#ifndef _CTI_FILE_H_
#define _CTI_FILE_H_

#include "CTI.h"
#include "Array.h"

extern ArrayU8 * File_load_data(const char *filename);

extern String * File_load_text(const char *filename);

#endif
