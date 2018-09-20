#include <stdio.h>
#include <string.h>
#include "CTI.h"
#include "VSmoother.h"
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"
#include "Log.h"

static void Config_handler(Instance *pi, void *msg);

/* VSmoother Instance and Template implementation. */
enum { INPUT_CONFIG  /* , INPUT_RGB3, INPUT_BGR3, INPUT_YUV422P */ };
static Input VSmoother_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

// enum { OUTPUT_RGB3, OUTPUT_BGR3, OUTPUT_YUV422P };
static Output VSmoother_outputs[] = {
};

/* Full type declaration. */
struct _VSmoother {
  Instance i;
  double period;
  //unsigned int factor;
  // double tlast;
  double last_ftime;
  double eta;
};


static Config config_table[] = {
};


void VSmoother_smooth(VSmoother *priv,
		      double frame_timestamp,
		      int pending_messages)
{
  /* Compare current frame time and previous frame time, and
     keep a running average of frame periods.  Sleep for difference
     between now and eta.  Re-calculate next eta.  Adjust sleep
     time and eta if pending_messages > 1.  */
  double tnow;
  double ftime;
  double ftdiff;

  cti_getdoubletime(&tnow);

  ftime = frame_timestamp;

  //if (!priv->factor) {
  //  priv->factor = 1;
  //  goto out;
  //}

  ftdiff = ftime - priv->last_ftime;

  printf("ftime=%.2f last_ftime=%.2f ftdiff=%.2f\n",
	 ftime, priv->last_ftime, ftdiff);

  if (ftdiff <= 0.0 || ftdiff > 10.0) {
    /* Unreasonable frame rates. */
    goto out;
  }

  if (priv->period == 0.0) {
    priv->period = ftdiff;
  }

  /* I think this is an IIR filter... */
#define IIR_SIZE 30
  priv->period = ((priv->period * (IIR_SIZE-1)) +  (ftdiff)) / IIR_SIZE;

  printf("tnow=%.6f eta=%.6f period=%.6f sleep=%.6f\n",
  	 tnow, priv->eta, priv->period, (priv->eta - tnow));

  if (tnow < priv->eta) {
    struct timespec ts;
    double x;

    ts.tv_sec = 0;
    /* Take difference, multiply by 1M to convert from decimal fraction
       to microseconds, then by 1000 to convert to nanoseconds. */
    ts.tv_nsec = ((priv->eta - tnow) * 1000000) * 1000;
    nanosleep(&ts, 0L);

    if (pending_messages <= 1) {
      /* Good, increment eta as expected. */
      priv->eta += priv->period;
    }
    else {
      /* Frames are starting to queue up, try to catch up by moving eta
	 closer in. */
      printf("catch up: %d\n", pending_messages);
      x = priv->period / pending_messages;
      priv->eta += x;
    }
  }
  else {
    /* Took too long to get here.  What to do?  Don't sleep.
       Set eta to current time plus period. "Catch up" code above
       should drain backed up frames at a faster rate. */
    printf("***\n");
    priv->eta = tnow + priv->period;
  }

 out:
  priv->last_ftime = ftime;
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void VSmoother_instance_init(Instance *pi)
{
  //VSmoother_private *priv = (VSmoother_private *)pi;
}

static Template VSmoother_template = {
  .label = "VSmoother",
  .priv_size = sizeof(VSmoother),
  .inputs = VSmoother_inputs,
  .num_inputs = table_size(VSmoother_inputs),
  .outputs = VSmoother_outputs,
  .num_outputs = table_size(VSmoother_outputs),
  // .tick = VSmoother_tick,
  .instance_init = VSmoother_instance_init,
};

void VSmoother_init(void)
{
  Template_register(&VSmoother_template);
}


