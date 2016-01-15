#ifndef _CTI_HTTPLIB_H_
#define _CTI_HTTPLIB_H_

#include "String.h"

extern String_list * cookies_from_env(void);
extern String * cookie_value(String_list * cookies, String * cookie_key);
extern String * cookie_generate();

extern String_list * queryvars_from_env(void);
extern String * queryvars_value(String_list * queryvars, String * key);

extern int http_path_info_match(String * teststr);

#endif
