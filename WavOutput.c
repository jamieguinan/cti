/* Search and replace "WavOutput" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "WavOutput.h"
#include "SourceSink.h"
#include "Wav.h"

static void Config_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_WAV };
static Input WavOutput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

//enum { /* OUTPUT_... */ };
static Output WavOutput_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String output;		/* File or host:port, used to intialize sink. */
  Sink *sink;
  int header_written;
} WavOutput_private;

static int set_output(Instance *pi, const char *value)
{
  WavOutput_private *priv = (WavOutput_private *)pi;

  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  String_set(&priv->output, value);
  priv->sink = Sink_new(sl(priv->output));

  return 0;
}

static Config config_table[] = {
  { "output", set_output , 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Wav_handler(Instance *pi, void *msg)
{
  WavOutput_private *priv = (WavOutput_private *)pi;
  Wav_buffer *wav = msg;

  dpf("Wav_handler %d/%d/%d\n", wav->params.channels, wav->params.rate, wav->params.bits_per_sample);

  if (wav->eof) {
    Sink_close_current(priv->sink);
    priv->sink = NULL;
  }

  if (!priv->sink) {
    return;
  }

  if (!priv->header_written) {
    int tmp = wav->data_length;
    wav->data_length = (1024*1024*2000);
    Wav_buffer_finalize(wav);
    wav->data_length = tmp;
    Sink_write(priv->sink, wav->header, wav->header_length);
    priv->header_written = 1;
  }

  Sink_write(priv->sink, wav->data, wav->data_length);

  Wav_buffer_discard(&wav);
}

static void WavOutput_tick(Instance *pi)
{
  Handler_message *hm;

  dpf("WavOutput_tick\n", 0);

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void WavOutput_instance_init(Instance *pi)
{
  // WavOutput_private *priv = (WavOutput_private *)pi;
}


static Template WavOutput_template = {
  .label = "WavOutput",
  .priv_size = sizeof(WavOutput_private),
  .inputs = WavOutput_inputs,
  .num_inputs = table_size(WavOutput_inputs),
  .outputs = WavOutput_outputs,
  .num_outputs = table_size(WavOutput_outputs),
  .tick = WavOutput_tick,
  .instance_init = WavOutput_instance_init,
};

void WavOutput_init(void)
{
  Template_register(&WavOutput_template);
}
