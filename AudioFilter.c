/* Search and replace "AudioFilter" with new module name. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "Audio.h"
#include "AudioFilter.h"

static void Config_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_WAV };
static Input AudioFilter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_WAV };
static Output AudioFilter_outputs[] = {
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
};

typedef struct {
  Instance i;

} AudioFilter_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Wav_handler(Instance *pi, void *msg)
{
  // AudioFilter_private *priv = (AudioFilter_private *)pi;
  Wav_buffer *wav = msg;



  if (pi->outputs[OUTPUT_WAV].destination) {
    PostData(wav, pi->outputs[OUTPUT_WAV].destination);
  }
}

static void AudioFilter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void AudioFilter_instance_init(Instance *pi)
{
  //AudioFilter_private *priv = (AudioFilter_private *)pi;
}


static Template AudioFilter_template = {
  .label = "AudioFilter",
  .priv_size = sizeof(AudioFilter_private),
  .inputs = AudioFilter_inputs,
  .num_inputs = table_size(AudioFilter_inputs),
  .outputs = AudioFilter_outputs,
  .num_outputs = table_size(AudioFilter_outputs),
  .tick = AudioFilter_tick,
  .instance_init = AudioFilter_instance_init,
};

void AudioFilter_init(void)
{
  Template_register(&AudioFilter_template);
}
