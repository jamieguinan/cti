#include "CTI.h"
#include <string.h>
#include <stdlib.h>
#include "Mem.h"

int Range_match_substring(Range *r, const char *substr)
{
  // int i;
  if (r->type != RANGE_STRINGS) {
    return -1;
  }

#if 0
  for (i=0; i < r->x.strings.descriptions.things_count; i++) {
    String *s = r->x.strings.descriptions.things[i];
    if (strstr(s->bytes, substr)) {
      return i;
    }
  }
#endif

  return -1;
}


Range *Range_new(int type)
{
  Range *r = Mem_calloc(1, sizeof(*r));
  r->type = type;
  return r;
}


void Range_clear(Range *range)
{
  ISet_clear(range->strings);
  ISet_clear(range->descriptions);
  memset(range, 0, sizeof(*range));
  Array_free(range->int_enums, int, NULL);
}

void Range_free(Range **range)
{
  Range_clear(*range);
  Mem_free(*range);
  *range = 0L;
}
