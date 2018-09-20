#ifndef _STATS_H_
#define _STATS_H_

#include <stdint.h>             /* uint64_t */
#include <sys/time.h>           /* gettimeofday */

/* Items per second tracking */
typedef struct {
  uint64_t total_items;
  struct {
    float value;
    uint64_t items_last;
    struct timeval tv_last;
  } records[3];  /* most recent, 1 sec, 5 sec */
} Items_per_sec;

extern void Items_per_sec_update(Items_per_sec *items_per_sec, int add_count);

#endif
