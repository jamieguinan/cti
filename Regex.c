#include <sys/types.h>
#include <regex.h>
#include <stdio.h>

#include "Regex.h"
#include "Mem.h"
#include "localptr.h"

int Regex_match(const char * input, const char * pattern)
{
  int rc;
  regex_t reg = {};
  regmatch_t matches[1] = {};

  rc = regcomp(&reg, pattern, REG_EXTENDED);
  if (rc != 0) {
    return 0;
  }

  rc = regexec(&reg, input, 1, matches, 0);

  regfree(&reg);

  if (rc == REG_NOMATCH) {
    return 0;
  }
  return 1;
}

String_list * Regex_get_matches(const char * input, const char * pattern)
{
  // fprintf(stderr, "%s\n", __func__);

  String_list * result = String_list_new();
  int rc;
  regex_t reg = {};

  // fprintf(stderr, "regcomp(%s)\n", pattern);
  rc = regcomp(&reg, pattern, REG_EXTENDED);
  if (rc != 0) {
    static struct {
      int err;
      const char * message;
    } errors [] = {
       { REG_BADBR, "Invalid use of back reference operator."},
       { REG_BADPAT,"Invalid use of pattern operators such as group or list."},
       { REG_BADRPT, "Invalid use of repetition operators such as using '*' as the first character."},
       { REG_EBRACE, "Un-matched brace interval operators."},
       { REG_EBRACK, "Un-matched bracket list operators."},
       { REG_ECOLLATE, "Invalid collating element."},
       { REG_ECTYPE, "Unknown character class name."},
       { REG_EEND, "Nonspecific error.  This is not defined by POSIX.2."},
       { REG_EESCAPE, "Trailing backslash."},
       { REG_EPAREN, "Un-matched parenthesis group operators."},
       { REG_ERANGE, "Invalid use of the range operator; for example, the ending point of the range, occurs prior to the starting point."},
       { REG_ESIZE, "Compiled regular expression requires a pattern buffer larger than  64Kb.   This is not defined by POSIX.2."},
       { REG_ESPACE, "The regex routines ran out of memory."},
       { REG_ESUBREG, "Invalid back reference to a subexpression."}
    };
    // fprintf(stderr, "regcomp rc=%d %s\n", rc, errors[rc].message);
    return result;
  }

  int offset = 0;
  while (1) {
    regmatch_t matches[2] = {};
    rc = regexec(&reg, input+offset, 2, matches, 0);
    if (rc != 0) {
      break;
    }
    int which = 0;
    if (matches[1].rm_so != -1) {
      /* Matched a group. */
      which = 1;
    }
    String * s = String_from_char(input+offset+matches[which].rm_so,
				  (matches[which].rm_eo - matches[which].rm_so));
    // fprintf(stderr, "%s\n", s(s));
    String_list_add(result, &s);
    offset += matches[0].rm_eo + 1;
  }

  return result;
}
