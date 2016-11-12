#include "String.h"
#include "Regex.h"
#include "serf_get.h"
#include "localptr.h"
#include <stdio.h>

void get_file(String * prefix, String * path)
{
}

void get_dir(String * host_port, String * path)
{
  int i;
  localptr(String, result) = String_new("");
  localptr(String, url) = String_sprintf("%s%s", s(host_port), s(path));
  serf_command_get(S("serf_get"), url, result);
  localptr(String, result2) = String_replace(result, "\n", "");
  localptr(String_list, matches) = Regex_get_matches(s(result2), "<a.href=\"([^\"]+)\"");
  for (i=0; i < String_list_len(matches); i++) {
    String * href = String_list_get(matches, i);
    if (String_eq(href, S(".."))) { continue; }
    printf("%s\n", s(href));
    if (String_ends_with(href, "/")) {
      get_dir(host_port, href);
    }
  }
}

int main()
{
  get_dir(S("http://192.168.1.11:8080"), S("/Files/"));
  
  return 0;
}
