#ifndef _CTI_STRING_JSMN_H_
#define _CTI_STRING_JSMN_H_

#include "jsmn.h"		/* Use -I cpp flag to find header. */

extern int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target);
extern String * String_dup_jsmn(String * json_text, jsmntok_t token);
extern int jsmn_get_int(String * json_text, jsmntok_t token, int * result);
extern void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_dump_verbose(String * json_text, jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_copy_skip(String * json_text, jsmntok_t * tokens, int num_tokens, int skip, FILE * dest);
extern String * jsmn_lookup_string(const char * key, String * json_text, jsmntok_t * tokens, int num_tokens);
extern int jsmn_lookup_int(const char * key, String * json_text, jsmntok_t * tokens, int num_tokens, int * value);

#define jsf(s, t) ((t).end-(t).start),((s)+(t).start)

typedef struct {
  const char * mainkey;
  String * (*handler)(String * json_text, jsmntok_t * tokens, int num_tokens);
} JsmnDispatchHandler;


extern String * jsmn_dispatch(const char * json_text, size_t json_text_length, 
			      const char * mainkey, JsmnDispatchHandler * handlers, int num_handlers);


extern int jsmn_parse_alloc(String * json_str, jsmntok_t ** tokens_ptr, int * num_tokens_ptr);
extern void jsmn_parse_free(jsmntok_t ** tokens_ptr, int * num_tokens_ptr);

typedef struct {
  String * js_str;
  jsmntok_t * tokens;
  int num_tokens;
  String * result;
} JsmnContext;

extern JsmnContext * JsmnContext_new(void);
#define JsmnContextTemp(t) JsmnContext * t __attribute__ ((__cleanup__(JsmnContext_free))) = JsmnContext_new()
extern void JsmnContext_free(JsmnContext **);

#endif
