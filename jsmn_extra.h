#ifndef _CTI_STRING_JSMN_H_
#define _CTI_STRING_JSMN_H_

#include "jsmn.h"		/* Use -I cpp flag to find header. */

extern int jsmn_extra_verbose;

extern int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target);
extern String * String_dup_jsmn(String * json_text, jsmntok_t token);
extern int jsmn_get_int(String * json_text, jsmntok_t token, int * result);
extern void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_dump_verbose(String * json_text, jsmntok_t * tokens, int num_tokens, int limit);
extern void jsmn_copy_skip(String * json_text, jsmntok_t * tokens, int num_tokens, int skip, FILE * dest);

#define jsf(s, t) ((t).end-(t).start),((s)+(t).start)

extern void jsmn_parse_alloc(String * json_str, jsmntok_t ** tokens_ptr, int * num_tokens_ptr);
extern void jsmn_parse_free(jsmntok_t ** tokens_ptr, int * num_tokens_ptr);

typedef struct {
  /* This is a context structure to encapsulate information used
     during parsing and dispatch. */
  String * js_str;
  jsmntok_t * tokens;
  int num_tokens;
  String * result;
} JsmnContext;

extern JsmnContext * JsmnContext_new(void);
extern void JsmnContext_parse(JsmnContext *jc, String * js);
extern void JsmnContext_free(JsmnContext **);

typedef struct {
  const char * firstkey;
  void (*handler)(JsmnContext * jc);
} JsmnDispatchHandler;

extern void jsmn_dispatch(JsmnContext * jc, const char * firstkey, 
			   JsmnDispatchHandler * handlers, int num_handlers);
extern String * jsmn_lookup_string(JsmnContext * jc, const char * key);
extern int jsmn_lookup_int(JsmnContext * jc, const char * key, int * value);

#endif
