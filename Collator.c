/* Collate inputs to a single output. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <float.h>		/* DBL_MAX */

#include "CTI.h"
#include "Collator.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void YUV420P_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV420P };
static Input Collator_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
};

enum { OUTPUT_YUV420P };
static Output Collator_outputs[] = {
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
};

#define MAX_BUFFERED 10
typedef struct {
  Instance i;
  YUV420P_buffer * bufs[MAX_BUFFERED];
  int count;
  int limit;
} Collator_private;


static Config config_table[] = {
  { "limit",  0L, 0L, 0L, cti_set_int, offsetof(Collator_private, limit) },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void YUV420P_handler(Instance *pi, void *msg)
{
  Collator_private *priv = (Collator_private *)pi;
  YUV420P_buffer *buffer = msg;
  int i, j;

  if (!pi->outputs[OUTPUT_YUV420P].destination) {
    /* No where to send, discard. */
    YUV420P_buffer_release(buffer);
    return;
  }

  for (i=0; i < MAX_BUFFERED; i++) {
    if (priv->bufs[i] == NULL) {
      priv->bufs[i] = buffer;
      break;
    }
  }

  priv->count += 1;

  if (priv->count == priv->limit) {
    for (j=0; j < priv->count; j++) {
      /* Find oldest in list. */
      double oldest_time = DBL_MAX;
      int oldest_index = 0;
      for (i=0; i < MAX_BUFFERED; i++) {
	if (priv->bufs[i] && priv->bufs[i]->c.timestamp < oldest_time) {
	  oldest_index = i;
	  oldest_time = priv->bufs[i]->c.timestamp;
	}
      }
      dpf("posting buffer timestamp %.2f\n", priv->bufs[oldest_index]->c.timestamp);
      PostData(priv->bufs[oldest_index], pi->outputs[OUTPUT_YUV420P].destination);
      priv->bufs[oldest_index] = 0; /* clear it */
    }
    priv->count = 0;
  }
}

static void Collator_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Collator_instance_init(Instance *pi)
{
  Collator_private *priv = (Collator_private *)pi;
  priv->limit = 3;
}


static Template Collator_template = {
  .label = "Collator",
  .priv_size = sizeof(Collator_private),
  .inputs = Collator_inputs,
  .num_inputs = table_size(Collator_inputs),
  .outputs = Collator_outputs,
  .num_outputs = table_size(Collator_outputs),
  .tick = Collator_tick,
  .instance_init = Collator_instance_init,
};

void Collator_init(void)
{
  Template_register(&Collator_template);
}
