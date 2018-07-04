/*
 * This is a utility module, not normally built into the cti main
 * program, but used by other programs.
 * 
 * Jsmn (https://github.com/zserge/jsmn) is a wonderfully small,
 * efficient JSON parser/tokenizer, and I am almost embarassed
 * to wrap it in this layer, but it is necessary to connect
 * it to my style and conventions with localptr().
 *
 * JsmnContext holds a set of the necessary variables for
 * calling jsmn_parse, and the code here automatically sizes
 * and allocates enough token space.
 */

#include <stdio.h>		/* printf */
#include "Mem.h"
#include "String.h"
#include "jsmn_extra.h"
#include "localptr.h"

int jsmn_extra_verbose = 0;

int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target)
{
  if (token.type == JSMN_STRING
      && (token.end - token.start) == strlen(target)
      && strncmp(s(json_text) + token.start, target, strlen(target)) == 0) {
    return 1;
  }
  else {
    return 0;
  }
}

String * String_dup_jsmn(String * json_text, jsmntok_t token)
{
  int slen = token.end - token.start;
  char temp[slen + 1];
  strncpy(temp, s(json_text)+token.start, slen);  temp[slen] = 0;
  return String_new(temp);
}

int jsmn_get_int(String * json_text, jsmntok_t token, int * value)
{
  int i;
  int x = 0;
  int m = 1;
  for (i=token.start; i < token.end; i++) {
    int c = s(json_text)[i];
    if (i == token.start && c == '-') {
      m = -1;
      continue;
    }
    if (c < '0' || c > '9') {
      return JSMN_ERROR_INVAL;
    }
    else {
      x = (x * 10) + (c - '0');
    }
  }

  *value = (x*m);

  return 0;
}

static const char * jsmn_type_map[] = {
  [ JSMN_UNDEFINED ] = "JSMN_UNDEFINED",
  [ JSMN_OBJECT ] = "JSMN_OBJECT",
  [ JSMN_ARRAY ] = "JSMN_ARRAY",
  [ JSMN_STRING ] = "JSMN_STRING",
  [ JSMN_PRIMITIVE ] = "JSMN_PRIMITIVE",
};

void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit)
{
  int i;
  fprintf(stderr, "%d jsmn tokens\n", num_tokens);
  for (i=0; i < num_tokens && (limit == 0 || i < limit); i++) {
    fprintf(stderr, "&& tokens[%d].type == %s\n", i, jsmn_type_map[tokens[i].type]);
    fprintf(stderr, "&& tokens[%d].size == %d\n", i, tokens[i].size);
  }
}

void jsmn_dump_verbose(String * json_text, jsmntok_t * tokens, int num_tokens, int limit)
{
  int i;
  fprintf(stderr, "%d jsmn tokens\n", num_tokens);
  for (i=0; i < num_tokens && (limit == 0 || i < limit); i++) {
    fprintf(stderr, "&& tokens[%d].type == %s", i, jsmn_type_map[tokens[i].type]);
    if (tokens[i].type == JSMN_STRING) {
      fprintf(stderr, " /* %.*s */", jsf(s(json_text), tokens[i]));
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "&& tokens[%d].size == %d\n", i, tokens[i].size);
  }
}

void jsmn_copy_skip(String * json_text, jsmntok_t * tokens, int num_tokens, int skip, FILE * dest)
{
  int i;
  fprintf(stderr, "jsmn_copy_skip...\n");
  const char * separator = "";
  for (i=skip; (i+1) < num_tokens; i+=2) {
    fprintf(stderr, "tokens %d and %d\n", i, i+1);
    if (tokens[i].type != JSMN_STRING) { break;  }
    if (tokens[i].size != 1) { break; }
    if (tokens[i+1].size != 0) { break; }
    fprintf(dest, "%s\"%.*s\":", separator, jsf(s(json_text), tokens[i]));
    switch (tokens[i+1].type) {
    case JSMN_STRING: fprintf(dest, "\"%.*s\"", jsf(s(json_text), tokens[i+1])); break;
    case JSMN_PRIMITIVE: fprintf(dest, "%.*s", jsf(s(json_text), tokens[i+1])); break;
    default: fprintf(dest, "\"\"");
    }
    separator = ",";
  }
}


int jsmn_lookup_int(JsmnContext * jc, const char * key, int * value)
{
  int i;

  for (i=0; i < (jc->num_tokens-1); i+=1) {
    if (jc->tokens[i].type == JSMN_ARRAY) {
      continue;
    }

    if (jc->tokens[i].type == JSMN_OBJECT) {
      continue;
    }

    if (jc->tokens[i].type == JSMN_STRING) {
      if (String_eq_jsmn(jc->js_str, jc->tokens[i], key)
          && jc->tokens[i+1].type == JSMN_PRIMITIVE)
        {
          return jsmn_get_int(jc->js_str, jc->tokens[i+1], value);
        }
      i += 1;
    }
  }
  return -1;
}


void jsmn_parse_alloc(String * json_str, jsmntok_t ** tokens_ptr, int * num_tokens_ptr,
		      int * allocated_tokens_ptr)
{
  jsmn_parser parser;
  int allocated_tokens = 8;
  jsmntok_t * tokens = Mem_malloc(allocated_tokens * sizeof(*tokens));
  int n;

  while (1) {
    jsmn_init(&parser);
    n = jsmn_parse(&parser, s(json_str), String_len(json_str), tokens, allocated_tokens);
    if (n >= 0) {
      //fprintf(stderr, "jsmn_dispatch sees %d tokens\n", n);
      *tokens_ptr = tokens;
      *num_tokens_ptr = n;
      *allocated_tokens_ptr = allocated_tokens;
      break;
    }
    else if (n == JSMN_ERROR_NOMEM) {
      allocated_tokens *= 2;
      //fprintf(stderr, "allocated_tokens=%d\n", allocated_tokens);
      tokens = Mem_realloc(tokens, allocated_tokens * sizeof(*tokens));
    }
    else {
      fprintf(stderr, "jsmn_parse error %d\n", n);
      break;
    }
  }
}

JsmnContext * JsmnContext_new(void)
{
  JsmnContext * jc = Mem_calloc(1, sizeof(*jc));
  jc->js_str = String_value_none();
  jc->result = String_value_none();
  return jc;
}

void JsmnContext_parse(JsmnContext *jc, String * js)
{
  jc->js_str = String_dup(js);
  jsmn_parse_alloc(js, &jc->tokens, &jc->num_tokens,  &jc->allocated_tokens);
}


void JsmnContext_free(JsmnContext ** pjc)
{
  if (*pjc) {
    JsmnContext * jc = *pjc;
    String_clear(&jc->js_str);
    if (jc->tokens) {
      Mem_free(jc->tokens);
    }
    String_clear(&jc->result);
    *pjc = NULL;
  }
}


void jsmn_dispatch(JsmnContext * jc, const char * firstkey,
		   JsmnDispatchHandler * handlers, int num_handlers)
{
  localptr(String, val1) = String_value_none();
  localptr(String, result) = String_value_none();
  jsmn_parser parser;
  int n;

  jc->allocated_tokens = 8;
  jc->tokens = Mem_malloc(jc->allocated_tokens * sizeof(*jc->tokens));

  while (1) {
    jsmn_init(&parser);
    n = jsmn_parse(&parser, s(jc->js_str), String_len(jc->js_str), jc->tokens, jc->allocated_tokens);
    if (n >= 0) {
      //fprintf(stderr, "jsmn_dispatch sees %d tokens\n", n);
      jc->num_tokens = n;
      break;
    }
    else if (n == JSMN_ERROR_NOMEM) {
      jc->allocated_tokens *= 2;
      //fprintf(stderr, "allocated_tokens=%d\n", allocated_tokens);
      jc->tokens = Mem_realloc(jc->tokens, jc->allocated_tokens * sizeof(*jc->tokens));
    }
    else {
      fprintf(stderr, "jsmn_parse error %d\n", n);
      break;
    }
  }
  
  // jsmn_dump_verbose(json_str, tokens, n, n);
  
  if (jc->num_tokens >= 3
      && jc->tokens[0].type == JSMN_OBJECT
      && jc->tokens[1].type == JSMN_STRING
      && jc->tokens[1].size == 1
      && jc->tokens[2].type == JSMN_STRING
      && jc->tokens[2].size == 0
      && String_eq_jsmn(jc->js_str, jc->tokens[1], firstkey)) {
    int i;
    for (i=0; i < num_handlers; i++) {
      if (String_eq_jsmn(jc->js_str, jc->tokens[2], handlers[i].firstkey)) {
	handlers[i].handler(jc);
	break;
      }
    }
  }
}


String * jsmn_lookup_string(JsmnContext * jc, const char * key)
{
  int i;

  if (jsmn_extra_verbose) {
    jsmn_dump_verbose(jc->js_str, jc->tokens, jc->num_tokens, jc->num_tokens);
  }

  for (i=0; i < (jc->num_tokens-1); i+=1) {
    if (jc->tokens[i].type == JSMN_ARRAY) {
      continue;
    }

    if (jc->tokens[i].type == JSMN_OBJECT) {
      continue;
    }

    if (jc->tokens[i].type == JSMN_STRING) {
      if (jc->tokens[i+1].type == JSMN_STRING &&
	  String_eq_jsmn(jc->js_str, jc->tokens[i], key)) {
	if (jsmn_extra_verbose) { fprintf(stderr, "found!\n"); }
	return String_dup_jsmn(jc->js_str, jc->tokens[i+1]);
      }
      i+=1;
    }
  }
  return String_value_none();
}

