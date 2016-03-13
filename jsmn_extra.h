#ifndef _CTI_STRING_JSMN_H_
#define _CTI_STRING_JSMN_H_

#include "jsmn.h"		/* Use -I cpp flag to find header. */

extern int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target);
extern String * String_dup_jsmn(String * json_text, jsmntok_t token);
extern int jsmn_get_int(String * json_text, jsmntok_t token, int * result);
extern void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_dump_verbose(String * json_text, jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_copy_skip(String * json_text, jsmntok_t * tokens, int num_tokens, int skip, FILE * dest);

#define jsf(s, t) ((t).end-(t).start),((s)+(t).start)

typedef struct {
  const char * cmdval;
  int num_args;
  IntStr * (*handler0)(void);
  const char * key1;
  IntStr * (*handler1)(const char * value1);
  const char * key2;
  IntStr * (*handler2)(const char  * value1, const char * value2);
} JsmnDispatchHandler;

extern IntStr * jsmn_dispatch(const char * json_text, size_t json_text_length, 
			      const char * cmdkey, JsmnDispatchHandler * handlers, int num_handlers);

#endif
