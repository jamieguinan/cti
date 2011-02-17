/*
 * Pack data into an Mpeg2 Transport Stream. 
 * Maybe unpack too?
 * See earlier code "modc/MpegTS.h" for starters.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
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
  char *output;		/* File or host:port, used to intialize sink. */
  Sink *sink;
} MpegTS_private;


static int set_output(Instance *pi, const char *value)
{
  MpegTS_private *priv = pi->data;

  if (priv->output) {
    free(priv->output);
  }
  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  priv->output = strdup(value);
  priv->sink = Sink_new(priv->output);

  return 0;
}


static Config config_table[] = {
  { "output", set_output , 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
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
  MpegTS_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
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
