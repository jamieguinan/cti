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
  unsigned long samplesToInput;
  unsigned long maxOutputBytes;
  uint8_t *output_buffer;
  // Audio_buffer *first_audio;	/* Keep this around for comparison with later blocks. */
  ArrayU8 * chunk;
  int num_samples;
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
  // int first_pass = 0;
  int rc;

  {
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
      printf("=== header.rate=%d\n", audio->header.rate);
      printf("=== header.channels=%d\n", audio->header.channels);
      printf("=== samplesToInput=%ld\n", priv->samplesToInput);
      printf("=== maxOutputBytes=%ld\n", priv->maxOutputBytes);
      printf("=== timestamp=%.6f\n", audio->timestamp);
      priv->output_buffer = Mem_malloc(priv->maxOutputBytes);
    }

    // first_pass = 1;
    
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


    //fc->mpegVersion = MPEG2;
    //fc->aacObjectType = MAIN;

    printf("=== mpegVersion: %s\n", mpegids[fc->mpegVersion]);
    printf("=== outputFormat: %s\n", fc->outputFormat ? "ADTS":"Raw");
    printf("=== channel_map: [%d %d %d %d ...]\n",
	   fc->channel_map[0], fc->channel_map[1],
	   fc->channel_map[2], fc->channel_map[3]);
    printf("=== objectType: %s\n", objectTypes[fc->aacObjectType]);
    printf("=== bitRrate: %ld\n", fc->bitRate);
    printf("=== quantqual: %ld\n", fc->quantqual);
    printf("=== inputFormat: %s\n", inputFormats[fc->inputFormat]);


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
    printf("=== faacEncSetConfiguration returns %d\n", rc);
    if (rc != 1) {
      printf("=== ERROR!!!!!!!!\n");
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
    printf("not enough samples (%d) to encode(%ld)\n", priv->num_samples, priv->samplesToInput);
    goto out;
  }
  
  while (priv->num_samples >= priv->samplesToInput) {
    int encoded = faacEncEncode(priv->fh,
				(int32_t*)(priv->chunk->data), priv->samplesToInput,
				priv->output_buffer, priv->maxOutputBytes);
    
    { 
      static FILE *f = NULL;
      if (!f) {
	f = fopen("test.aac", "wb");
      }
      if (f) {
	if (fwrite(priv->output_buffer, 1, encoded, f) != encoded) { perror("fwrite test.aac"); }
      }
    }
    
    // printf("<- encoded %d bytes\n",  encoded);
    if (encoded <= 0) {
      sleep(1);
      continue;
    }
    
    if (pi->outputs[OUTPUT_AAC].destination) {
      AAC_buffer * aac = AAC_buffer_from(priv->output_buffer, encoded);
      aac->timestamp = audio->timestamp;
      PostData(aac, pi->outputs[OUTPUT_AAC].destination);
    }
    
    priv->num_samples -= priv->samplesToInput;
    ArrayU8_trim_left(priv->chunk, priv->samplesToInput * sizeof(int16_t));
  }

 out:
#if 0
  if (first_pass) {
    priv->first_audio = audio;
  }
  else {
  }
#else
  Audio_buffer_discard(&audio);
#endif
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
