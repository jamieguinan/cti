/*
 * This is a utility module, not normally built into the cti main
 * program, but used by other programs.
 */
#include <stdio.h>		/* printf */
#include "Mem.h"
#include "String.h"
#include "jsmn_extra.h"

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


String * jsmn_lookup_string(const char * key, String * json_text, jsmntok_t * tokens, int num_tokens)
{
  int i;
  for (i=3; i < (num_tokens-1); i+=2) {
    printf("%s: %.*s (want %s) %.*s (%d %d)\n",
	   __func__, 
	   jsf(s(json_text), tokens[i]), 
	   key,
	   jsf(s(json_text), tokens[i+1]),
	   tokens[i+1].type, JSMN_STRING);
    if (String_eq_jsmn(json_text, tokens[i], key)
	&& tokens[i+1].type == JSMN_STRING) {
      printf("found!\n");
      return String_dup_jsmn(json_text, tokens[i+1]);
    }
  }
  return String_value_none();
}


int jsmn_lookup_int(const char * key, String * json_text, jsmntok_t * tokens, int num_tokens, int * value)
{
  int i;
  for (i=3; i < (num_tokens-1); i+=2) {
    printf("%s: %.*s (want %s) %.*s (%d %d)\n",
	   __func__, 
	   jsf(s(json_text), tokens[i]), 
	   key,
	   jsf(s(json_text), tokens[i+1]),
	   tokens[i+1].type, JSMN_PRIMITIVE);
    if (String_eq_jsmn(json_text, tokens[i], key)
	&& tokens[i+1].type == JSMN_PRIMITIVE) {
      return jsmn_get_int(json_text, tokens[i+1], value);
    }
  }
  return -1;
}


int jsmn_parse_alloc(String * json_str, jsmntok_t ** tokens_ptr, int * num_tokens_ptr)
{
  jsmn_parser parser;
  int num_tokens = 8;
  jsmntok_t * tokens = Mem_malloc(num_tokens * sizeof(*tokens));
  int n;

  while (1) {
    jsmn_init(&parser);
    n = jsmn_parse(&parser, s(json_str), String_len(json_str), tokens, num_tokens);
    if (n >= 0) {
      //fprintf(stderr, "jsmn_dispatch sees %d tokens\n", n);
      *tokens_ptr = tokens;
      *num_tokens_ptr = num_tokens;
      break;
    }
    else if (n == JSMN_ERROR_NOMEM) {
      num_tokens *= 2;
      //fprintf(stderr, "num_tokens=%d\n", num_tokens);
      tokens = Mem_realloc(tokens, num_tokens * sizeof(*tokens));
    }
    else {
      fprintf(stderr, "jsmn_parse error %d\n", n);
      break;
    }
  }
  return n;
}

void jsmn_parse_free(jsmntok_t ** tokens_ptr, int * num_tokens_ptr)
{
  if (*tokens_ptr) {
    Mem_free(*tokens_ptr);
    *tokens_ptr = NULL;
  }
  *num_tokens_ptr = 0;
}


String * jsmn_dispatch(const char * json_text, size_t json_text_length, 
			const char * mainkey, JsmnDispatchHandler * handlers, int num_handlers)
{
  jsmn_parser parser;
  int num_tokens = 8;
  jsmntok_t * tokens = Mem_malloc(num_tokens * sizeof(*tokens));
  int n;
  String * val1 = String_value_none();
  String * json_str = String_from_char(json_text, json_text_length);
  String * result = String_value_none();

  while (1) {
    jsmn_init(&parser);
    n = jsmn_parse(&parser, json_text, json_text_length, tokens, num_tokens);
    if (n >= 0) {
      //fprintf(stderr, "jsmn_dispatch sees %d tokens\n", n);
      break;
    }
    else if (n == JSMN_ERROR_NOMEM) {
      num_tokens *= 2;
      //fprintf(stderr, "num_tokens=%d\n", num_tokens);
      tokens = Mem_realloc(tokens, num_tokens * sizeof(*tokens));
    }
    else {
      fprintf(stderr, "jsmn_parse error %d\n", n);
      goto out;
    }
  }
  
  // jsmn_dump_verbose(json_str, tokens, n, n);
  
  if (n >= 3
      && tokens[0].type == JSMN_OBJECT
      && tokens[1].type == JSMN_STRING
      && tokens[1].size == 1
      && tokens[2].type == JSMN_STRING
      && tokens[2].size == 0
      && String_eq_jsmn(json_str, tokens[1], mainkey)) {
    int i;
    for (i=0; i < num_handlers; i++) {
      if (String_eq_jsmn(json_str, tokens[2], handlers[i].mainkey)) {
	result = handlers[i].handler(json_str, tokens, n);
	break;
      }
    }
  }

  
 out:
  String_clear(&json_str);
  String_clear(&val1);
  if (tokens) {
    Mem_free(tokens);
  }
  return result;
}


JsmnContext * JsmnContext_new(void)
{
  JsmnContext * jc = Mem_calloc(1, sizeof(*jc));
  jc->js_str = String_value_none();
  jc->result = String_value_none();
  return jc;
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
  }
  *pjc = NULL;
}
