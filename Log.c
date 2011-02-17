#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "Log.h"
#include "Cfg.h"

#define NUM_ENTRIES 100

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {
  int index;
  char *msgs[NUM_ENTRIES];
} Logs[LOG_NUM_CATEGORIES] = {};

void Log(int category, char *fmt, ...)
{
  if (!cfg.logging) {
    return;
  }

  int n;
  int rc;

  char *p = 0L;
  va_list ap;
  va_start(ap, fmt);
  rc = vasprintf(&p, fmt, ap);
  if (-1 == rc) {
    perror("vasprintf");
  }
  va_end(ap);

  pthread_mutex_lock(&log_lock);

  n = Logs[category].index;
  if (Logs[category].msgs[n]) {
    free(Logs[category].msgs[n]);
  }
  
  Logs[category].msgs[n] = p;

  n += 1;
  if (n == NUM_ENTRIES) {
    n = 0;
  }
  Logs[category].index = n;

  pthread_mutex_unlock(&log_lock);
}


void Log_dump(void)
{
  int category;
  for (category = 0; category < LOG_NUM_CATEGORIES; category++) {
    int a, b, i;
    int n = Logs[category].index;
    if (Logs[category].msgs[NUM_ENTRIES-1]) {
      /* Buffer has wrapped, start at next position. */
      a = n;
      b = (n - 1 + NUM_ENTRIES) % NUM_ENTRIES;
    }
    else {
      a = 0;
      b = n;
    }

    for (i=a; i != b; i = (i + 1) % NUM_ENTRIES) {
      printf("Logs[%d].msgs[%d]: %s\n", category, i, Logs[category].msgs[i]);
    }
  }
}

