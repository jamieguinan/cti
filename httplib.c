#include <stdio.h>		/* fopen */
#include <stdlib.h>		/* getenv */
#include <stdint.h>		/* uint32_t */
#include "String.h"

String_list * cookies_from_env(void)
{
  char * cookies = getenv("HTTP_COOKIE");
  if (!cookies) {
    return String_list_value_none();
  }

  String_list * result = String_split_s(cookies, "; ");
  String_list_trim(result);
    
  return result;
}

String * cookie_value(String_list * cookies, String * cookie_key)
{
  /* Using the result of cookies_from_env(), return the value of a particular cookie. */
  int i;
  for (i=0; i < String_list_len(cookies); i++) {
    int end = 0;
    String * kv = String_list_get(cookies, i);
    //printf("String_find(%s, %d, %s, end@%p\n", s(kv), 0, s(cookie_key), &end);
    int start = String_find(kv, 0, s(cookie_key), &end);
    //printf("start=%d end=%d\n", start, end);
    if (start == 0 && end > 0 && s(kv)[end] == '=') {
      return String_new(s(cookie_key)+end);
    }
  }
  return String_value_none();
}


String * cookie_generate()
{
  String * result = String_value_none();
  FILE * f = fopen("/dev/urandom", "rb");
  if (!f) {
    return result;
  }

  uint32_t values[4];
  if (fread(values, 1, sizeof(values), f) == sizeof(values)) {
    result = String_sprintf("%04x%04x%04x%04x", values[0], values[1], values[2], values[3]);
  }

  fclose(f);
  return result;
}
