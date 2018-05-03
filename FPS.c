#include <stdio.h>		/* NULL */
#include "FPS.h"
#include "Cfg.h"
#include "CTI.h"

void FPS_update_timestamp(FPS *fps, double timestamp)
{
  double p;

  if (fps->timestamp_last == 0.0) {
    goto out;
  }

  p = timestamp - fps->timestamp_last;

  if (fps->period == 0.0) {
    fps->period = p;
  }
  else {
    fps->period = ((fps->period * 19) + (p))/20;
    if (++fps->count % 10 == 0) {
      dpf("p=%.3f %.3f fps\n", p, (1.0/fps->period));
    }
  }

 out:
  fps->timestamp_last = timestamp;
}


void FPS_update(FPS *fps)
{
  double timestamp;
  cti_getdoubletime(&timestamp);
  FPS_update_timestamp(fps, timestamp);
  
}
