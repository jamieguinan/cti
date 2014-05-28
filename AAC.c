/* 
 * AAC encoding using "faac" library. 
 * Reference,
 *   file:///usr/share/doc/faac-1.28-r4/pdf/libfaac.pdf
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "AAC.h"
#include "Audio.h"
#include "Array.h"

#include "faac.h"

static void Config_handler(Instance *pi, void *msg);
static void Audio_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_AUDIO, INPUT_WAV };
static Input AAC_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_AUDIO ] = { .type_label = "Audio_buffer", .handler = Audio_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_AAC  };
static Output AAC_outputs[] = {
  [ OUTPUT_AAC ] = { .type_label = "AAC_buffer", .destination = 0L },
};

typedef struct {
  Instance i;

  int rate_per_channel;

  faacEncHandle fh;
  unsigned long samplesToInput;
  unsigned long maxOutputBytes;
  uint8_t *output_buffer;
  // Audio_buffer *first_audio;	/* Keep this around for comparison with later blocks. */
  ArrayU8 * chunk;
  int num_samples;

} AAC_private;

static Config config_table[] = {
  { "rate_per_channel",  0L, 0L, 0L, cti_set_int, offsetof(AAC_private, rate_per_channel) },
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
  int rc;
  
  if (0) {
    static FILE *f = NULL;
    if (!f) {
      f = fopen("test.raw", "wb");
    }
    if (f) {
      if (fwrite(audio->data, 1, audio->data_length, f) != audio->data_length) { perror("fwrite test.aac"); }
    }
  }


  if (!priv->fh) {
    priv->fh = faacEncOpen(audio->header.rate,
			   audio->header.channels,
			   &priv->samplesToInput,
			   &priv->maxOutputBytes
			   );  
    if (!priv->fh) {
      printf("%s: faacEncOpen returned NULL\n", __func__);
      return;
    }
    else {
      printf("\n");
      printf("aac: header.rate=%d\n", audio->header.rate);
      printf("aac: header.channels=%d\n", audio->header.channels);
      printf("aac: samplesToInput=%ld\n", priv->samplesToInput);
      printf("aac: maxOutputBytes=%ld\n", priv->maxOutputBytes);
      printf("aac: timestamp=%.6f\n", audio->timestamp);
      priv->output_buffer = Mem_malloc(priv->maxOutputBytes);
    }

    faacEncConfigurationPtr fc = faacEncGetCurrentConfiguration(priv->fh);
    fc->inputFormat = FAAC_INPUT_FLOAT;
    static char * inputFormats[] = {
      [FAAC_INPUT_NULL] = "FAAC_INPUT_NULL",
      [FAAC_INPUT_16BIT] = "FAAC_INPUT_16BIT",
      [FAAC_INPUT_24BIT] = "FAAC_INPUT_24BIT",
      [FAAC_INPUT_32BIT] = "FAAC_INPUT_32BIT",
      [FAAC_INPUT_FLOAT] = "FAAC_INPUT_FLOAT",
    };
    static char * mpegids[] = { [MPEG2] = "MPEG2", [MPEG4] = "MPEG4" };
    static char * objectTypes[] = {
      [0] = "UNSET",
      [MAIN] = "MAIN", 
      [LOW] = "LOW", 
      [SSR] = "SSR",
      [LTP] = "LTP",
    };

    fc->mpegVersion = MPEG4;
    fc->aacObjectType = LOW;
    fc->bitRate = priv->rate_per_channel;

    printf("aac: mpegVersion: %s\n", mpegids[fc->mpegVersion]);
    printf("aac: outputFormat: %s\n", fc->outputFormat ? "ADTS":"Raw");
    printf("aac: channel_map: [%d %d %d %d ...]\n",
	   fc->channel_map[0], fc->channel_map[1],
	   fc->channel_map[2], fc->channel_map[3]);
    printf("aac: objectType: %s\n", objectTypes[fc->aacObjectType]);
    printf("aac: bitRrate: %ld\n", fc->bitRate);
    printf("aac: quantqual: %ld\n", fc->quantqual);
    printf("aac: inputFormat: %s\n", inputFormats[fc->inputFormat]);


    switch(audio->header.atype) {
    case CTI_AUDIO_16BIT_SIGNED_LE: 
      fc->inputFormat = FAAC_INPUT_16BIT;
      break;
    case CTI_AUDIO_8BIT_SIGNED_LE:
    case CTI_AUDIO_24BIT_SIGNED_LE:
      fprintf(stderr, "unsupported audio format\n");
      return;
    default:
      fprintf(stderr, "invalid audio format\n");
      return;
    }
    
    
    rc = faacEncSetConfiguration(priv->fh, fc);
    printf("aac: faacEncSetConfiguration returns %d\n", rc);
    if (rc != 1) {
      printf("aac: ERROR!!!!!!!!\n");
    }
    
  }

#if 0
  if (priv->first_audio) {
    if (priv->first_audio->header.rate != audio->header.rate ||
	priv->first_audio->header.channels != audio->header.channels) {
      fprintf(stderr, "AAC Audio_handler cannot change format mid-stream!\n");
      return;
    }
  }
#endif

  /* Append new samples. */
  ArrayU8_append(priv->chunk, ArrayU8_temp_const(audio->data, audio->data_length));
  priv->num_samples += audio->data_length/sizeof(int16_t);

  if (priv->num_samples < priv->samplesToInput) {
    dpf("not enough samples (%d) to encode(%ld)\n", priv->num_samples, priv->samplesToInput);
    goto out;
  }

  double nominal_period = 
	priv->samplesToInput	   /* Total samples. */
	* 1.0			   /* Convert to float */
	// /audio->header.frame_size) /* Number of frames */
	/audio->header.rate;	   /* frames/sec */
  double timestamp = audio->timestamp;
  
  while (priv->num_samples >= priv->samplesToInput) {
    int encoded = faacEncEncode(priv->fh,
				(int32_t*)(priv->chunk->data), priv->samplesToInput,
				priv->output_buffer, priv->maxOutputBytes);
    
    if (1) { 
      static FILE *f = NULL;
      if (!f) {
	f = fopen("test.aac", "wb");
      }
      if (f) {
	if (fwrite(priv->output_buffer, 1, encoded, f) != encoded) { perror("fwrite test.aac"); }
      }
    }
    
    //printf("AAC encoded %d bytes, timestamp=%.3f\n",  encoded, timestamp);
    if (encoded <= 0) {
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);
      continue;
    }
    
    if (pi->outputs[OUTPUT_AAC].destination) {
      AAC_buffer * aac = AAC_buffer_from(priv->output_buffer, encoded);
      aac->timestamp = timestamp;
      aac->nominal_period = nominal_period;
      PostData(aac, pi->outputs[OUTPUT_AAC].destination);
    }
    
    priv->num_samples -= priv->samplesToInput;

    timestamp += nominal_period;

    ArrayU8_trim_left(priv->chunk, priv->samplesToInput * sizeof(int16_t));
  }

  //printf("AAC done with audio buffer\n");

 out:
  Audio_buffer_discard(&audio);
}


static void Wav_handler(Instance *pi, void *msg)
{
  Wav_buffer *wav = msg;

  /* Pass along to audio handler. */
  Audio_buffer *audio = Audio_buffer_new(wav->params.rate, wav->params.channels, wav->params.atype);
  Audio_buffer_add_samples(audio, wav->data, wav->data_length);
  audio->timestamp = wav->timestamp;
  Audio_handler(pi, audio);
  Wav_buffer_release(&wav);
}


static void AAC_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void AAC_instance_init(Instance *pi)
{
  AAC_private *priv = (AAC_private *)pi;
  priv->chunk = ArrayU8_new();
}


static Template AAC_template = {
  .label = "AAC",
  .priv_size = sizeof(AAC_private),
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
