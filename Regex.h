#ifndef _CTI_REGEX_H_
#define _CTI_REGEX_H_

#include "String.h"

extern int Regex_match(const char * input, const char * pattern);

extern String_list * Regex_get_matches(const char * input, const char * pattern);

#endif
