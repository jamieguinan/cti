#include <stdio.h>		/* NULL */
#include "FPS.h"
#include "Cfg.h"

void FPS_show(FPS *fps)
{
  double t1, t2, p;
  struct timeval tv;
  gettimeofday(&tv, NULL);

  if (!fps->tv_last.tv_sec) {
    fps->tv_last.tv_sec = tv.tv_sec;
    fps->tv_last.tv_usec = tv.tv_usec;
    goto out;
  }

  t1 = fps->tv_last.tv_sec + (fps->tv_last.tv_usec/1000000.0);
  t2 = tv.tv_sec + (tv.tv_usec/1000000.0);
  p = t2 - t1;

  if (fps->period == 0.0) {
    fps->period = p;
  }
  else {
    fps->period = ((fps->period * 19) + (p))/20;
    if (++fps->count % 10 == 0 && cfg.verbosity) {
      printf("p=%.3f %.3f fps\n", p, (1.0/fps->period));
    }
  }

 out:
  fps->tv_last = tv;
}
