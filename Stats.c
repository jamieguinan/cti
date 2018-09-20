#include "Stats.h"
#include <stdio.h>              /* NULL */
#include <inttypes.h>           /* PRIu64 */

static int thresholds[3] = { 1000, 3000, 5000 };

void Items_per_sec_update(Items_per_sec *items_per_sec, int add_count)
{
  int i;
  struct timeval tv_now;

  gettimeofday(&tv_now, NULL);

  items_per_sec->total_items += add_count;

  for (i=0; i < 3; i++) {
    if (items_per_sec->records[i].tv_last.tv_sec == 0) {
      items_per_sec->records[i].tv_last = tv_now;
      continue;
    }
    else {
      long tdiff_milliseconds =
        ((tv_now.tv_sec - items_per_sec->records[i].tv_last.tv_sec) * 1000)
        + ((tv_now.tv_usec - items_per_sec->records[i].tv_last.tv_usec) / 1000);

      if (tdiff_milliseconds == 0) { continue; }
      if (tdiff_milliseconds >= thresholds[i]) {
        if (0) printf("tdiff_milliseconds=%ld, X=%" PRIu64 " %.1f %.1f\n",
               tdiff_milliseconds,
               items_per_sec->total_items - items_per_sec->records[i].items_last,
               1000.0 * (items_per_sec->total_items - items_per_sec->records[i].items_last),
               (1000.0*(items_per_sec->total_items - items_per_sec->records[i].items_last))/tdiff_milliseconds
               );

        items_per_sec->records[i].value =
          (1000.0*(items_per_sec->total_items - items_per_sec->records[i].items_last))/tdiff_milliseconds;
        items_per_sec->records[i].tv_last = tv_now;
        items_per_sec->records[i].items_last = items_per_sec->total_items;
      }
    }
  }

}
