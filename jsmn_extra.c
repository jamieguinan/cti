/*
 * This is a utility module, not normally built into the cti main
 * program, but used by other programs.
 */
#include <stdio.h>		/* printf */
#include <stdlib.h>		/* malloc, realloc */
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

int jsmn_get_int(String * json_text, jsmntok_t token, int * result)
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

  *result = (x*m);

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


String * jsmn_dispatch(const char * json_text, size_t json_text_length, 
		       const char * cmdkey, JsmnDispatchHandler * handlers, int num_handlers)
{
  jsmn_parser parser;
  int num_tokens = 8;
  jsmntok_t * tokens = malloc(num_tokens * sizeof(*tokens));
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
      tokens = realloc(tokens, num_tokens * sizeof(*tokens));
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
      && String_eq_jsmn(json_str, tokens[1], cmdkey)) {
    int i;
    for (i=0; i < num_handlers; i++) {
      if (String_eq_jsmn(json_str, tokens[2], handlers[i].cmdval)) {
	if (n == 3 
	    && handlers[i].num_args == 0
	    && handlers[i].handler0
	    ) {
	  result = handlers[i].handler0();
	  break;
	}
	else if (n == 5 
		 && handlers[i].num_args == 1
		 && handlers[i].key1
		 && handlers[i].handler1
		 && tokens[3].type == JSMN_STRING
		 && tokens[3].size == 1
		 && tokens[4].type == JSMN_STRING
		 && tokens[4].size == 0) {
	  val1 = String_dup_jsmn(json_str, tokens[4]);
	  result = handlers[i].handler1(s(val1));
	  break;
	}
      }
    }
  }

  
 out:
  String_clear(&json_str);
  String_clear(&val1);
  if (tokens) {
    free(tokens);
  }
  return result;
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


String * jsmn_dispatch2(const char * json_text, size_t json_text_length, 
			const char * mainkey, JsmnDispatchHandler2 * handlers, int num_handlers)
{
  jsmn_parser parser;
  int num_tokens = 8;
  jsmntok_t * tokens = malloc(num_tokens * sizeof(*tokens));
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
      tokens = realloc(tokens, num_tokens * sizeof(*tokens));
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
    free(tokens);
  }
  return result;
}
