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
    String * kv = String_list_get(cookies, i);
    String_list * k_v = String_split(kv, "=");
    if (String_list_len(k_v) == 2 && String_eq(String_list_get(k_v, 0), cookie_key)) {
      return String_list_get(k_v, 1);
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
    result = String_sprintf("%08x%08x%08x%08x", values[0], values[1], values[2], values[3]);
  }

  fclose(f);
  return result;
}


String_list * queryvars_from_env(void)
{
  char * queryvars = getenv("QUERY_STRING");
  if (!queryvars) {
    return String_list_value_none();
  }

  String_list * result = String_split_s(queryvars, "&");
  String_list_trim(result);

  return result;
}


String * queryvars_value(String_list * queryvars, String * key)
{
  int i;
  for (i=0; i < String_list_len(queryvars); i++) {
    String * kv = String_list_get(queryvars, i);
    String_list * k_v = String_split(kv, "=");
    if (String_list_len(k_v) == 2 && String_eq(String_list_get(k_v, 0), key)) {
      return String_list_get(k_v, 1);
    }
  }
  return String_value_none();
}


int http_path_info_match(String * teststr)
{
  char * path_info = getenv("PATH_INFO");
  if (path_info && String_eq(teststr, S(path_info))) {
    return 1;
  }
  return 0;
}
