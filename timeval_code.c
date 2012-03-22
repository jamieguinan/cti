
static inline double tv_to_double(struct timeval *tv) {
  return (tv->tv_sec + tv->tv_usec/1000000.0);
}

static inline int tv_lt_diff(struct timeval *tv1, struct timeval *tv2, struct timespec *diff)
{
  if (tv1->tv_sec > tv2->tv_sec ||
      (tv1->tv_sec == tv2->tv_sec && tv1->tv_usec > tv2->tv_usec)) {
    diff->tv_sec = 0;
    diff->tv_nsec = 0;
    return 0;
  }

  diff->tv_sec = tv2->tv_sec - tv1->tv_sec;
  if (tv2->tv_usec > tv1->tv_usec) {
    /* Straight subtraction. */
    diff->tv_nsec = (tv2->tv_usec - tv1->tv_usec)*1000;
  }
  else {
    /* Subtract with borrow. */
    diff->tv_nsec = (1000000 + tv2->tv_usec - tv1->tv_usec)*1000;
    diff->tv_sec -= 1;
  }

  return 1;
}

static inline int tv_gt_diff(struct timeval *tv1, struct timeval *tv2, struct timespec *diff)
{
  if (tv1->tv_sec < tv2->tv_sec ||
      (tv1->tv_sec == tv2->tv_sec && tv1->tv_usec < tv2->tv_usec)) {
    diff->tv_sec = 0;
    diff->tv_nsec = 0;
    return 0;
  }

  diff->tv_sec = tv1->tv_sec - tv2->tv_sec;
  if (tv1->tv_usec > tv2->tv_usec) {
    /* Straight subtraction. */
    diff->tv_nsec = (tv1->tv_usec - tv2->tv_usec)*1000;
  }
  else {
    /* Subtract with borrow. */
    diff->tv_nsec = (1000000 + tv1->tv_usec - tv2->tv_usec)*1000;
    diff->tv_sec -= 1;
  }

  return 1;
}

static inline void tv_ts_add(struct timeval *tv, struct timespec *ts, struct timeval *tv_out)
{
  tv_out->tv_sec = tv->tv_sec + ts->tv_sec;
  tv_out->tv_usec = tv->tv_usec + (ts->tv_nsec/1000);
  if (tv_out->tv_usec > 1000000) {
    /* Carry. */
    tv_out->tv_usec -= 1000000;
    tv_out->tv_sec += 1;
  }
}

static inline void tv_clear(struct timeval *tv)
{
  tv->tv_sec = 0;
  tv->tv_usec = 0;
}


#if 0
/* FIXME: Use this or delete it. */
static void handle_playback_timing(SDLstuff_private *priv, struct timeval *tv_current)
{
  struct timeval now;
  struct timespec ts;
  double d;

  d = fabs(tv_to_double(&priv->tv_last) - tv_to_double(tv_current));

  if (d > 1.0) {
    /* tv_last is unset, or tv_current has changed substantially. */
    fprintf(stderr, "d:%.3f too big\n", d);
    goto out1;
    return;
  }

  gettimeofday(&now, 0L);

  if (tv_lt_diff(&now, &priv->tv_sleep_until, &ts)) {
    fprintf(stderr, "nanosleep %ld.%09ld\n", ts.tv_sec, ts.tv_nsec);
    if (nanosleep(&ts, 0L) != 0) { perror("nanosleep"); }
  }
  else {
    // fprintf(stderr, "no sleep\n");
    /* FIXME: If we took too long more than once, should do something... */
    gettimeofday(&priv->tv_sleep_until, 0L);
  }

  if (tv_gt_diff(tv_current, &priv->tv_last, &ts)) {
    /* Normally expect this. */
    tv_ts_add(&priv->tv_sleep_until, &ts, &priv->tv_sleep_until);
    // fprintf(stderr, "normal; sleep until { %ld.%06ld } \n",  priv->tv_sleep_until.tv_sec, priv->tv_sleep_until.tv_usec);
  }
  else {
    /* tv_current is unexpectedly behind tv_last.  Clear sleep_until and hope things
       work out. */
  }

 out1:
  priv->tv_last = *tv_current;
}
#endif
