/*
 * Pack data into an Mpeg2 Transport Stream. 
 * Maybe unpack too?
 * See earlier code "modc/MpegTS.h" for starters.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "MpegTS.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input MpegTS_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output MpegTS_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String output;		/* File or host:port, used to intialize sink. */
  Sink *sink;
} MpegTS_private;


static int set_output(Instance *pi, const char *value)
{
  MpegTS_private *priv = (MpegTS_private *)pi;

  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  String_set(&priv->output, value);
  priv->sink = Sink_new(s(priv->output));

  return 0;
}


static Config config_table[] = {
  { "output", set_output , 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void MpegTS_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void MpegTS_instance_init(Instance *pi)
{
  MpegTS_private *priv = (MpegTS_private *)pi;
  
}


static Template MpegTS_template = {
  .label = "MpegTS",
  .inputs = MpegTS_inputs,
  .num_inputs = table_size(MpegTS_inputs),
  .outputs = MpegTS_outputs,
  .num_outputs = table_size(MpegTS_outputs),
  .tick = MpegTS_tick,
  .instance_init = MpegTS_instance_init,
};

void MpegTS_init(void)
{
  Template_register(&MpegTS_template);
}
