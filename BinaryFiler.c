#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "BinaryFiler.h"
#include "Images.h"
#include "Cfg.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);
static void H264_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_H264 };
static Input BinaryFiler_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_H264 ] = { .type_label = "H264_buffer", .handler = H264_handler },
};

//enum { /* OUTPUT_... */ };
static Output BinaryFiler_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  Sink * sink;
} BinaryFiler_private;

static int set_output(Instance *pi, const char *value)
{
  BinaryFiler_private *priv = (BinaryFiler_private *)pi;
  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }

  if (priv->sink) {
    Sink_free(&priv->sink);
  }
  priv->sink = Sink_new(value);

  return 0;
}


static Config config_table[] = {
{ "output",    set_output, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void H264_handler(Instance *pi, void *data)
{
  BinaryFiler_private *priv = (BinaryFiler_private *)pi;
  H264_buffer *h264 = data;
  dpf("%s: %d bytes\n", __func__, h264->encoded_length);

  if (priv->sink) {
    Sink_write(priv->sink, h264->data, h264->encoded_length);
  }

  H264_buffer_release(h264);
  pi->counter += 1;
}

static void BinaryFiler_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}


static void BinaryFiler_instance_init(Instance *pi)
{
}

static Template BinaryFiler_template = {
  .label = "BinaryFiler",
  .priv_size = sizeof(BinaryFiler_private),
  .inputs = BinaryFiler_inputs,
  .num_inputs = table_size(BinaryFiler_inputs),
  .outputs = BinaryFiler_outputs,
  .num_outputs = table_size(BinaryFiler_outputs),
  .tick = BinaryFiler_tick,
  .instance_init = BinaryFiler_instance_init,
};

void BinaryFiler_init(void)
{
  Template_register(&BinaryFiler_template);
}
