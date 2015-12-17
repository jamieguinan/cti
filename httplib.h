#ifndef _CTI_HTTPLIB_H_
#define _CTI_HTTPLIB_H_

#include "String.h"

extern String_list * cookies_from_env(void);
extern String * cookie_value(String_list * cookies, String * cookie_key);
extern String * cookie_generate();

#endif
