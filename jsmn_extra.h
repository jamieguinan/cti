#ifndef _CTI_STRING_JSMN_H_
#define _CTI_STRING_JSMN_H_

#include "jsmn.h"		/* Use -I cpp flag to find header. */

extern int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target);
extern String * String_dup_jsmn(String * json_text, jsmntok_t token);
extern int jsmn_get_int(String * json_text, jsmntok_t token, int * result);
extern void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit);

#define jsf(s, t) ((t).end-(t).start),((s)+(t).start)

#endif
