/* Provide a single YUV422P buffer repeatedly, for benchmarking. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Y4MSource.h"
#include "Images.h"
#include "File.h"
#include "Cfg.h"

static void y422p_handler(Instance *pi, void *msg);
static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV422P };
static Input Y4MSource_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_YUV422P, OUTPUT_CONFIG };
static Output Y4MSource_outputs[] = {
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  YUV422P_buffer *y4m;
} Y4MSource_private;


static int do_run(Instance *pi, const char *value)
{
  /* Clones and posts the y4m buffer N times.  N=atoi(value). */
  Y4MSource_private *priv = (Y4MSource_private *)pi;
  int count = atoi(value);
  int i;

  printf("%s:%s\n", __FILE__, __func__);

  if (!priv->y4m) {
    fprintf(stderr, "Y4MSource has no y4m data!\n");
    return 1;
  }

  if (!pi->outputs[OUTPUT_YUV422P].destination) {
    fprintf(stderr, "Y4MSource has no destination!\n");
    return 1;
  }

  for (i=0; i < count; i++) {
    YUV422P_buffer *tmp = YUV422P_copy(priv->y4m, 0, 0, priv->y4m->width, priv->y4m->height);

    if (cfg.verbosity) {
      printf("%d/%d (%d)\n", i, count,
	     pi->outputs[OUTPUT_YUV422P].destination->parent->pending_messages);
    }

    while (pi->outputs[OUTPUT_YUV422P].destination->parent->pending_messages > 5) {
      /* Throttle output.  25ms sleep. */
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 25 * 1000 * 1000}, NULL);
    }

    PostData(tmp, pi->outputs[OUTPUT_YUV422P].destination);
  }

  if (pi->outputs[OUTPUT_CONFIG].destination) {
    PostData(Config_buffer_new("quit", "0"), pi->outputs[OUTPUT_CONFIG].destination);
  }

  return 0;
}


static Config config_table[] = {
  { "run",     do_run, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  printf("%s:%s\n", __FILE__, __func__);
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void y422p_handler(Instance *pi, void *data)
{
  Y4MSource_private *priv = (Y4MSource_private *)pi;
  priv->y4m = data;
  printf("%s: y4m=%p\n", __func__, priv->y4m);
}


static void Y4MSource_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Y4MSource_instance_init(Instance *pi)
{
  // Y4MSource_private *priv = (Y4MSource_private *)pi;
}


static Template Y4MSource_template = {
  .label = "Y4MSource",
  .priv_size = sizeof(Y4MSource_private),
  .inputs = Y4MSource_inputs,
  .num_inputs = table_size(Y4MSource_inputs),
  .outputs = Y4MSource_outputs,
  .num_outputs = table_size(Y4MSource_outputs),
  .tick = Y4MSource_tick,
  .instance_init = Y4MSource_instance_init,
};

void Y4MSource_init(void)
{
  Template_register(&Y4MSource_template);
}
