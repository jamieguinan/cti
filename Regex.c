#include <sys/types.h>
#include <regex.h>

#include "Regex.h"

int Regex_match(const char * input, const char * pattern)
{
  int rc;
  regex_t reg = {};
  regmatch_t matches[3] = {};

  rc = regcomp(&reg, pattern, REG_EXTENDED);
  if (rc != 0) {
    return 0;
  }

  rc = regexec(&reg, input, 3, matches, 0);
  if (rc == REG_NOMATCH) {
    return 0;
  }
  return 1;
}
