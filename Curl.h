#ifndef _CTI_CURL_H_
#define _CTI_CURL_H_

#include "String.h"

extern void Curl_init(void);
extern void post(const char * uri, String_list * headers, String * data,
                 void (*handler)(int code, String_list * headers, String * data));


#endif
