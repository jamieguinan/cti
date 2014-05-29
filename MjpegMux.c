/*
 * MJpeg and WAV audio muxer.  Output is not a standard container,
 * just a sequence of Jpeg frames (JFIF) and Wav blocks, prefixed with
 * HTTP headers.  Use other tools to convert to AVI, OGG, etc.
 */
#include <string.h>		/* memcpy */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* free */

#include "CTI.h"
#include "String.h"
#include "Images.h"
// #include "XMixedReplace.h"
#include "Wav.h"
#include "Audio.h"
#include "SourceSink.h"
#include "Mem.h"
#include "Signals.h"

static void Config_handler(Instance *pi, void *data);
static void Jpeg_handler(Instance *pi, void *data);
static void O511_handler(Instance *pi, void *data);
static void Wav_handler(Instance *pi, void *data);
static void Audio_handler(Instance *pi, void *data);
static void AAC_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_JPEG, INPUT_O511, INPUT_WAV, INPUT_AUDIO };
static Input MjpegMux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_buffer", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_O511 ] = { .type_label = "O511_buffer", .handler = O511_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
  [ INPUT_AUDIO ] = { .type_label = "Audio_buffer", .handler = Audio_handler },
  [ INPUT_AUDIO ] = { .type_label = "AAC_buffer", .handler = AAC_handler },
};

enum { OUTPUT_RAWDATA };
static Output MjpegMux_outputs[] = {
  [ OUTPUT_RAWDATA ] = { .type_label = "RawData_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  String output;		/* File or host:port, used to intialize sink. */
  Sink *sink;
  int seq;
  int every;
} MjpegMux_private;


static int set_output(Instance *pi, const char *value)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;
  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  fprintf(stderr, "set_output(%s)\n", value);

  String_set_local(&priv->output, value);
  priv->sink = Sink_new(sl(priv->output));
  priv->every = 1;

  return 0;
}


static int do_restart(Instance *pi, const char *value)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;
  if (sl(priv->output)) {
    /* Use a copy of the string, because priv->output.bytes will get 
       deleted in String_set_local()  */
    char output[strlen(sl(priv->output))+1];
    strcpy(output, sl(priv->output));
    set_output(pi, output);
  }

  return 0;
}


static int set_every(Instance *pi, const char *value)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;
  priv->every = atoi(value);
  return 0;
}


static Config config_table[] = {
  { "output", set_output, 0L, 0L },
  { "restart", do_restart, 0L, 0L },
  { "every", set_every, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

#define BOUNDARY "--0123456789NEXT"

static const char part_format[] = 
  "%s\r\nContent-Type: %s\r\n"
  "Timestamp:%.6f\r\n"
  "%s"				/* extra headers... */
  "Content-Length: %d\r\n\r\n";

static void Jpeg_handler(Instance *pi, void *data)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;  
  Jpeg_buffer *jpeg_in = data;

  priv->seq += 1;

  if (priv->every && (priv->seq % priv->every != 0)) {
    goto out;
  }

  String *period;
  if (jpeg_in->c.nominal_period > 0.00001) {
    period = String_sprintf("Period:%.6f\r\n", jpeg_in->c.nominal_period);
  }
  else {
    period = String_sprintf("");
  }

  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "image/jpeg",
				  jpeg_in->c.timestamp,
				  s(period),
				  jpeg_in->encoded_length);


  if (priv->sink) {
    /* Format header. */
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, jpeg_in->data, jpeg_in->encoded_length);
  }
  
  if (pi->outputs[OUTPUT_RAWDATA].destination) {
    /* Combine header and data, post. */
    RawData_buffer *raw = RawData_buffer_new(header->len + jpeg_in->encoded_length);
    memcpy(raw->data, header->bytes, header->len);
    memcpy(raw->data+header->len, jpeg_in->data, jpeg_in->encoded_length);
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }

  String_free(&period);
  String_free(&header);

 out:
  Jpeg_buffer_discard(jpeg_in);
}


static void O511_handler(Instance *pi, void *data)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;  
  O511_buffer *o511_in = data;
  /* Format header. */
  String *dimensions = String_sprintf("Width:%d\r\nHeight:%d\r\n", o511_in->width, o511_in->height);
  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "image/o511",
				  o511_in->c.timestamp,
				  dimensions->bytes,
				  o511_in->encoded_length);
  
  if (priv->sink) {
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, o511_in->data, o511_in->encoded_length);
  }

  if (pi->outputs[OUTPUT_RAWDATA].destination) {
    /* Combine header and data, post. */
    RawData_buffer *raw = RawData_buffer_new(header->len + o511_in->encoded_length);
    memcpy(raw->data, header->bytes, header->len);
    memcpy(raw->data+header->len, o511_in->data, o511_in->encoded_length);    
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }

  String_free(&dimensions);
  String_free(&header);
  O511_buffer_discard(o511_in);
}


static void Wav_handler(Instance *pi, void *data)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;  
  Wav_buffer *wav_in = data;

  dpf("%s wav_in %p, pi->outputs[OUTPUT_RAWDATA].destination=%p\n", __func__, wav_in,
      pi->outputs[OUTPUT_RAWDATA].destination);

  /* Format header. */
  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "audio/x-wav",
				  wav_in->timestamp,
				  "",
				  wav_in->header_length+wav_in->data_length);

  if (priv->sink) {
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, wav_in->header, wav_in->header_length);
    Sink_write(priv->sink, wav_in->data, wav_in->data_length);
  }

  if (pi->outputs[OUTPUT_RAWDATA].destination) {
    /* Combine header and data, post. */
    RawData_buffer *raw = RawData_buffer_new(header->len + wav_in->header_length + wav_in->data_length);
    memcpy(raw->data, header->bytes, header->len);
    memcpy(raw->data+header->len, wav_in->header, wav_in->header_length);
    memcpy(raw->data+header->len+wav_in->header_length, wav_in->data, wav_in->data_length);
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }

  String_free(&header);
  // printf("%s discarding wav_in @ %p (%d bytes @ %p)\n", __func__, wav_in, wav_in->data_length, wav_in->data);
  Wav_buffer_release(&wav_in);
}


static void AAC_handler(Instance *pi, void *data)
{
  MjpegMux_private *priv = (MjpegMux_private *)pi;  
  AAC_buffer *aac = data;

  /* Format header. */
  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "audio/aac",
				  aac->timestamp,
				  "",
				  aac->data_length);

  if (priv->sink) {
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, aac->data, aac->data_length);
  }

  if (pi->outputs[OUTPUT_RAWDATA].destination) {
    /* Combine header and data, post. */
    RawData_buffer *raw = RawData_buffer_new(header->len + aac->data_length);
    memcpy(raw->data, header->bytes, header->len);
    memcpy(raw->data+header->len, aac->data, aac->data_length);
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }

  String_free(&header);
  AAC_buffer_discard(&aac);
}


static void Audio_handler(Instance *pi, void *data)
{
  fprintf(stderr, "%s not implemented yet!\n", __func__);
}

static void MjpegMux_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}

static void MjpegMux_instance_init(Instance *pi)
{
}

static Template MjpegMux_template = {
  .label = "MjpegMux",
  .priv_size = sizeof(MjpegMux_private),
  .inputs = MjpegMux_inputs,
  .num_inputs = table_size(MjpegMux_inputs),
  .outputs = MjpegMux_outputs,
  .num_outputs = table_size(MjpegMux_outputs),
  .tick = MjpegMux_tick,
  .instance_init = MjpegMux_instance_init,
};

void MjpegMux_init(void)
{
  Template_register(&MjpegMux_template);
}
