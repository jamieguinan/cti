#include <stdio.h>
#include <string.h>
#include <sys/time.h>		/* gettimeofday */
#include "CTI.h"
#include "VSmoother.h"
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"
#include "Log.h"

#define _min(a, b)  ((a) < (b) ? (a) : (b))
#define _max(a, b)  ((a) > (b) ? (a) : (b))

static void Config_handler(Instance *pi, void *msg);
static void rgb3_handler(Instance *pi, void *msg);
static void bgr3_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

/* VSmoother Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_RGB3, INPUT_BGR3, INPUT_422P };
static Input VSmoother_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = rgb3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = bgr3_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_RGB3, OUTPUT_BGR3, OUTPUT_422P };
static Output VSmoother_outputs[] = { 
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_BGR3 ] = {.type_label = "BGR3_buffer", .destination = 0L },
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  double period;
  double sum;
  unsigned int factor;
  // double tlast;
  double last_ftime;
  double eta;
} VSmoother_private;


static Config config_table[] = {
};


static void smooth(VSmoother_private *priv, 
		   struct timeval *frame_timestamp, 
		   int pending_messages)
{
  /* Compare current frame time and previous frame time, and
     keep a running average of frame periods.  Sleep for difference
     between now and eta.  Re-calculate next eta.  Adjust sleep
     time and eta if pending_messages > 1.  */
  double tnow;
  double ftime;
  double ftdiff;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  tnow = tv.tv_sec + (tv.tv_usec/1000000.0);

  ftime = frame_timestamp->tv_sec + (frame_timestamp->tv_usec/1000000.0);

  if (!priv->factor) {
    priv->factor = 1;
    goto out;
  }

  ftdiff = ftime - priv->last_ftime;
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

static void rgb3_handler(Instance *pi, void *data)
{
  VSmoother_private *priv = pi->data;
  RGB3_buffer *rgb3_in = data;
  if (pi->outputs[OUTPUT_RGB3].destination) {
    smooth(priv, &rgb3_in->tv, pi->pending_messages);
    PostData(rgb3_in, pi->outputs[OUTPUT_RGB3].destination);
  }
  else {
    RGB3_buffer_discard(rgb3_in);
  }
}

static void bgr3_handler(Instance *pi, void *data)
{
  VSmoother_private *priv = pi->data;
  BGR3_buffer *bgr3_in = data;
  if (pi->outputs[OUTPUT_BGR3].destination) {
    smooth(priv, &bgr3_in->tv, pi->pending_messages);
    PostData(bgr3_in, pi->outputs[OUTPUT_BGR3].destination);
  }
  else {
    BGR3_buffer_discard(bgr3_in);
  }
}

static void y422p_handler(Instance *pi, void *data)
{
  VSmoother_private *priv = pi->data;
  Y422P_buffer *y422p_in = data;
  if (pi->outputs[OUTPUT_422P].destination) {
    smooth(priv, &y422p_in->tv,pi->pending_messages);
    PostData(y422p_in, pi->outputs[OUTPUT_422P].destination);
  }
  else {
    Y422P_buffer_discard(y422p_in);
  }
}


static void VSmoother_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void VSmoother_instance_init(Instance *pi)
{
  VSmoother_private *priv = Mem_calloc(1, sizeof(VSmoother_private));
  pi->data = priv;
}

static Template VSmoother_template = {
  .label = "VSmoother",
  .inputs = VSmoother_inputs,
  .num_inputs = table_size(VSmoother_inputs),
  .outputs = VSmoother_outputs,
  .num_outputs = table_size(VSmoother_outputs),
  .tick = VSmoother_tick,
  .instance_init = VSmoother_instance_init,  
};

void VSmoother_init(void)
{
  Template_register(&VSmoother_template);
}


