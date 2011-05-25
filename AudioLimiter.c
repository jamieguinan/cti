/* Search and replace "AudioLimiter" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Wav.h"

static void Config_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_WAV };
static Input AudioLimiter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_WAV };
static Output AudioLimiter_outputs[] = {
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
};

typedef struct {
  int limit;
} AudioLimiter_private;


static int set_limit(Instance *pi, const char *value)
{
  AudioLimiter_private *priv = pi->data;
  priv->limit = atoi(value);
  if ((priv->limit > 100) || (priv->limit < 0)) {
    fprintf(stderr, "limit must be in 0..100 range, setting to 0\n");
    priv->limit = 0;
  }
  else {
    fprintf(stderr, "audio limit set to %d/100\n", priv->limit);
  }
  return 0;
}


static Config config_table[] = {
  { "limit",  set_limit, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Wav_handler(Instance *pi, void *msg)
{
  Wav_buffer *wav_in = msg;
  AudioLimiter_private *priv = pi->data;

  if (!pi->outputs[OUTPUT_WAV].destination) {
      Wav_buffer_discard(&wav_in);
      return;
  }

  if (priv->limit) {
    int i;
    int16_t sample;
    for (i=0; i < wav_in->data_length; i+= sizeof(int16_t))  {
      sample = (wav_in->data[i+1] << 8) + wav_in->data[i];
      sample = (sample * priv->limit) / 100;
      wav_in->data[i] = sample & 0xff;
      wav_in->data[i+1] = (sample >> 8) & 0xff;
    }
  }

  /* Forward message to destination. */
  PostData(wav_in, pi->outputs[OUTPUT_WAV].destination);
}


static void AudioLimiter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void AudioLimiter_instance_init(Instance *pi)
{
  AudioLimiter_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template AudioLimiter_template = {
  .label = "AudioLimiter",
  .inputs = AudioLimiter_inputs,
  .num_inputs = table_size(AudioLimiter_inputs),
  .outputs = AudioLimiter_outputs,
  .num_outputs = table_size(AudioLimiter_outputs),
  .tick = AudioLimiter_tick,
  .instance_init = AudioLimiter_instance_init,
};

void AudioLimiter_init(void)
{
  Template_register(&AudioLimiter_template);
}
