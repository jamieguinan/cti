/* 
 * This module buffers single media frames, and either discards them
 * or passes them to an MjpegMux instance, which should either save or
 * forward.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <sys/time.h>		/* gettimeofday */

#include "CTI.h"
#include "MjpegLocalBuffer.h"
#include "Images.h"
#include "Wav.h"
#include "MotionDetect.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *data);
static void Wav_handler(Instance *pi, void *data);
static void MotionDetect_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_RAWDATA, INPUT_JPEG, INPUT_WAV, INPUT_MOTION_DETECT };
static Input MjpegLocalBuffer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  /* Known-format data is sent from local objects. */
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
  [ INPUT_MOTION_DETECT ] =  { .type_label = "MotionDetect_result", .handler = MotionDetect_handler },
};

enum { OUTPUT_JPEG, OUTPUT_WAV, OUTPUT_CONFIG  };
static Output MjpegLocalBuffer_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

#define STORE_SIZE 1024

typedef struct {
  Instance i;
  int forwardbuffer; /* Seconds to keep recording after last trigger */
  struct timeval record_until;
  int recording;
  int md_threshold;
} MjpegLocalBuffer_private;


static Config config_table[] = {
  { "forwardbuffer",  0L, 0L, 0L, cti_set_int, offsetof(MjpegLocalBuffer_private, forwardbuffer) },
  { "md_threshold",  0L, 0L, 0L, cti_set_int, offsetof(MjpegLocalBuffer_private, md_threshold) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void check_record_state(MjpegLocalBuffer_private *priv)
{
  if (priv->recording) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec > priv->record_until.tv_sec) {
      priv->recording = 0;
      fprintf(stderr, "md recording OFF (%lu > %lu)\n", 
	      (unsigned long)tv.tv_sec, (unsigned long)priv->record_until.tv_sec);
    }
  }
}

static void MotionDetect_handler(Instance *pi, void *data)
{
  MjpegLocalBuffer_private *priv = (MjpegLocalBuffer_private *)pi;
  MotionDetect_result *md = data;

  check_record_state(priv);

  dpf("md->sum=%d (%d)\n", md->sum, priv->md_threshold);

  if (priv->recording) {
    fprintf(stderr, "md recording ON (%d %d)\n", md->sum, priv->md_threshold);
  }

  if (md->sum > priv->md_threshold) {
    gettimeofday(&priv->record_until, NULL);
    priv->record_until.tv_sec += priv->forwardbuffer;
    if (!priv->recording) {
      priv->recording = 1;
      if (pi->outputs[OUTPUT_CONFIG].destination) {
	/* Start new output. */
	PostData(Config_buffer_new("restart", "1"), pi->outputs[OUTPUT_CONFIG].destination);
      }
    }
  }
}

static void Jpeg_handler(Instance *pi, void *data)
{
  MjpegLocalBuffer_private *priv = (MjpegLocalBuffer_private *)pi;
  Jpeg_buffer *jpeg = data;

  check_record_state(priv);

  /* If recording, pass along to MjpegMux output, else discard. */
  if (priv->recording && pi->outputs[OUTPUT_JPEG].destination) {
    PostData(jpeg, pi->outputs[OUTPUT_JPEG].destination);    
  }
  else {
    Jpeg_buffer_discard(jpeg);
  }
}


static void Wav_handler(Instance *pi, void *data)
{
  MjpegLocalBuffer_private *priv = (MjpegLocalBuffer_private *)pi;
  Wav_buffer *wav = data;

  check_record_state(priv);

  /* If recording, pass along to MjpegMux output, else discard. */
  if (priv->recording && pi->outputs[OUTPUT_WAV].destination) {
    PostData(wav, pi->outputs[OUTPUT_WAV].destination);    
  }
  else {
    Wav_buffer_discard(&wav);
  }
}



static void MjpegLocalBuffer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void MjpegLocalBuffer_instance_init(Instance *pi)
{
  MjpegLocalBuffer_private *priv = (MjpegLocalBuffer_private *)pi;
  
  priv->md_threshold = 1000000;
  priv->forwardbuffer = 5;
}


static Template MjpegLocalBuffer_template = {
  .label = "MjpegLocalBuffer",
  .priv_size = sizeof(MjpegLocalBuffer_private),
  .inputs = MjpegLocalBuffer_inputs,
  .num_inputs = table_size(MjpegLocalBuffer_inputs),
  .outputs = MjpegLocalBuffer_outputs,
  .num_outputs = table_size(MjpegLocalBuffer_outputs),
  .tick = MjpegLocalBuffer_tick,
  .instance_init = MjpegLocalBuffer_instance_init,
};

void MjpegLocalBuffer_init(void)
{
  Template_register(&MjpegLocalBuffer_template);
}
