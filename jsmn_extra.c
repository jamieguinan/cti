/*
 * This is a utility module, not normally built into the cti main
 * program, but used by other programs.
 */
#include <stdio.h>		/* printf */
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
