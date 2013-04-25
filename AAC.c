/* AAC encoding using "faac" library. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "AAC.h"
#include "Audio.h"

#include "faac.h"

static void Config_handler(Instance *pi, void *msg);
static void Audio_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_AUDIO };
static Input AAC_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_AUDIO ] = { .type_label = "Audio_buffer", .handler = Audio_handler },
};

enum { OUTPUT_AAC  };
static Output AAC_outputs[] = {
  [ OUTPUT_AAC ] = { .type_label = "AAC_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  faacEncHandle fh;
  uint8_t output_buffer[1024];
  Audio_buffer *first_audio;	/* Keep this around for comparison with later blocks. */
} AAC_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Audio_handler(Instance *pi, void *msg)
{
  AAC_private *priv = (AAC_private *)pi;
  Audio_buffer *audio = msg;
  unsigned long inputSamples = 2048;
  unsigned long maxOutputBytes = sizeof(priv->output_buffer);
  int first_pass = 0;

  if (!priv->fh) {
    priv->fh = faacEncOpen(audio->rate,
			   audio->channels,
			   &inputSamples, /* This parameter just seems stupid. */
			   &maxOutputBytes /* So does this one. */
			   );  
    if (!priv->fh) {
      return;
    }

    first_pass = 1;

    faacEncConfigurationPtr fc = faacEncGetCurrentConfiguration(priv->fh);

    switch(audio->type) {
    case CTI_AUDIO_16BIT_SIGNED_LE:
      fc->inputFormat = FAAC_INPUT_16BIT;
      break;
    case CTI_AUDIO_8BIT_SIGNED_LE:
    case CTI_AUDIO_24BIT_SIGNED_LE:
      fprintf(stderr, "unhandled audio format\n");
      return;
    default:
      fprintf(stderr, "invalid audio format\n");
      return;
    }

    faacEncSetConfiguration(priv->fh, fc);
  }

  if (priv->first_audio) {
    if (priv->first_audio->rate != audio->rate ||
	priv->first_audio->channels != audio->channels) {
      fprintf(stderr, "AAC Audio_handler cannot change format mid-stream!\n");
      return;
    }
  }

  unsigned int samples = audio->data_length/(audio->frame_size);
  unsigned int samples_sent = 0;
  
  while (samples_sent < samples) {
    unsigned int samples_to_send = cti_min(samples, inputSamples);
    faacEncEncode(priv->fh,
		  (int32_t*)(audio->data + (samples_sent*audio->frame_size)), samples_to_send,
		  priv->output_buffer, sizeof(priv->output_buffer));
    samples -= samples_to_send;
    samples_sent += samples_to_send;
  }

  if (first_pass) {
    priv->first_audio = audio;
  }
  else {
    Audio_buffer_discard(&audio);
  }
}


static void AAC_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void AAC_instance_init(Instance *pi)
{
}


static Template AAC_template = {
  .label = "AAC",
  .inputs = AAC_inputs,
  .num_inputs = table_size(AAC_inputs),
  .outputs = AAC_outputs,
  .num_outputs = table_size(AAC_outputs),
  .tick = AAC_tick,
  .instance_init = AAC_instance_init,
};

void AAC_init(void)
{
  Template_register(&AAC_template);
}
