/*
 * This program mirrors files from the "Filer" iOS app, which has a
 * built-in HTTP server.
 *
 *   http://hammockdistrict.com/filer
 *
 * Filer is my favorite iOS app. It provides a simple file
 * that you can put stuff in. No frills.
 */

#include "String.h"
#include "Regex.h"
#include "serf_get.h"
#include "localptr.h"
#include <stdio.h>

void get_file(String * host_port, String * path)
{
  localptr(String, outfile) = String_sprintf(".%s", s(path));
  localptr(String, url) = String_sprintf("%s%s", s(host_port), s(path));
  printf("Getting %s to %s\n", s(url), s(outfile));  
  //serf_command_get(S("serf_get"), url, outfile);
}

void get_dir(String * host_port, String * path)
{
  int i;
  localptr(String, result) = String_new("");
  localptr(String, url) = String_sprintf("%s%s", s(host_port), s(path));
  serf_command_get(S("serf_get"), url, result);
  localptr(String_list, matches) = Regex_get_matches(s(result), "<a.href=\"([^\"]+)\"");
  for (i=0; i < String_list_len(matches); i++) {
    String * href = String_list_get(matches, i);
    if (String_eq(href, S(".."))) { continue; }
    if (String_ends_with(href, "/")) {
      get_dir(host_port, href);
    }
    else {
      get_file(host_port, href);      
    }
  }
}

int main()
{
  get_dir(S("http://192.168.1.11:8080"), S("/Files/"));
  
  return 0;
}
