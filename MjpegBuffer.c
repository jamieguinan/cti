/* 
 * This module handles buffering and saving stream data to disk.
 * I might split this into MjpegLocalBuffer, and MjpegStreamBuffer,
 * since there appear to be two non-overlapping use cases...
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <sys/time.h>		/* gettimeofday */

#include "CTI.h"
#include "MjpegBuffer.h"
#include "Images.h"
#include "Wav.h"

static void Config_handler(Instance *pi, void *msg);
static void RawData_handler(Instance *pi, void *data);
static void Jpeg_handler(Instance *pi, void *data);
static void Wav_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_RAWDATA, INPUT_JPEG, INPUT_WAV };
static Input MjpegBuffer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  /* Raw data is forwarded from MjpegDemux. */
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
  /* Known-format data is sent from local objects. */
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

//enum { /* OUTPUT_... */ };
static Output MjpegBuffer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

#define STORE_SIZE 1024

typedef struct {
  Instance i;
  String *basename;
  int backbuffer;		/* Seconds of buffered data to flush when recording turned on */
  int forwardbuffer;		/* Seconds to keep recording after last trigger */
  struct timeval record_until;
  int recording;
  FILE *output;
  struct {
    RawData_buffer *raw;
    struct timeval tv;
  } store[STORE_SIZE];
  int put;
  int get;
  int seq;
} MjpegBuffer_private;

static int do_trigger(Instance *pi, const char *value_unused)
{
  MjpegBuffer_private *priv = (MjpegBuffer_private *)pi;
  gettimeofday(&priv->record_until, NULL);
  priv->record_until.tv_sec += priv->forwardbuffer;
  return 0;
}


static int set_basename(Instance *pi, const char *value)
{
  MjpegBuffer_private *priv = (MjpegBuffer_private *)pi;
  if (priv->basename) {
    String_free(&priv->basename);
  }
  priv->basename = String_new(value);
  return 0;
}


static Config config_table[] = {
  { "trigger",     do_trigger, 0L, 0L },
  { "basename",    set_basename, 0L, 0L },
  { "backbuffer",  0L, 0L, 0L, cti_set_int, offsetof(MjpegBuffer_private, back_buffer) },
  { "forwardbuffer",  0L, 0L, 0L, cti_set_int, offsetof(MjpegBuffer_private, forward_buffer) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void RawData_handler(Instance *pi, void *data)
{ 
  MjpegBuffer_private *priv = (MjpegBuffer_private *)pi;
  RawData_buffer *raw = data;

  if (priv->put == priv->get) {
    priv->get += 1;
  }
  
  RawData_buffer_discard(raw);
}


static void Jpeg_handler(Instance *pi, void *data)
{
  Jpeg_buffer *jpeg = data;
  /* Update record state.  */
  /* if recording, pass along to MjpegMux output, else discard. */
  Jpeg_buffer_discard(jpeg);
}


static void Wav_handler(Instance *pi, void *data)
{
  Wav_buffer *wav = data;
  /* Update record state.  */
  /* If recording, pass along to MjpegMux output, else discard. */
  Wav_buffer_discard(&wav);
}



static void MjpegBuffer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void MjpegBuffer_instance_init(Instance *pi)
{
  MjpegBuffer_private *priv = (MjpegBuffer_private *)pi;
  
}


static Template MjpegBuffer_template = {
  .label = "MjpegBuffer",
  .priv_size = sizeof(MjpegBuffer_private),
  .inputs = MjpegBuffer_inputs,
  .num_inputs = table_size(MjpegBuffer_inputs),
  .outputs = MjpegBuffer_outputs,
  .num_outputs = table_size(MjpegBuffer_outputs),
  .tick = MjpegBuffer_tick,
  .instance_init = MjpegBuffer_instance_init,
};

void MjpegBuffer_init(void)
{
  Template_register(&MjpegBuffer_template);
}
