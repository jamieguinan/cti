/*
 * Repair timestamps in mjx streams from earlier versions of my code, where I was generating
 * timestamps with "%d.%d instead of "%d.%06d".
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "MjxRepair.h"
#include "SourceSink.h"
#include "Images.h"
#include "Wav.h"
#include "Log.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *data);
static void Wav_handler(Instance *pi, void *data);


enum { INPUT_CONFIG, INPUT_JPEG, INPUT_WAV };
static Input MjxRepair_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_JPEG, OUTPUT_WAV };
static Output MjxRepair_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
};

#define BUFFER_SIZE 15
typedef struct {
  Instance i;
  String output;		/* File or host:port, used to intialize sink. */
  Sink *sink;

  struct {
    Jpeg_buffer jpeg;
    Wav_buffer wav;
  } buffer[BUFFER_SIZE];
  int buffer_first;
  int buffer_last;
} MjxRepair_private;

static int set_output(Instance *pi, const char *value)
{
  MjxRepair_private *priv = (MjxRepair_private *)pi;
  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  String_set(&priv->output, value);
  priv->sink = Sink_new(s(priv->output));

  return 0;
}

static Config config_table[] = {
  { "output", set_output , 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


#define BOUNDARY "--0123456789NEXT"

static const char part_format[] = 
  "%s\r\nContent-Type: %s\r\n"
  "Timestamp:%ld.%06ld\r\n"  	/* very important to use six digits for microseconds! :) */
  "%s"				/* extra headers... */
  "Content-Length: %d\r\n\r\n";

static void Jpeg_handler(Instance *pi, void *data)
{
  MjxRepair_private *priv = (MjxRepair_private *)pi;  
  Jpeg_buffer *jpeg_in = data;

  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "image/jpeg",
				  jpeg_in->c.tv.tv_sec, jpeg_in->c.tv.tv_usec,
				  "",
				  jpeg_in->encoded_length);


  if (priv->sink) {
    /* Format header. */
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, jpeg_in->data, jpeg_in->encoded_length);
  }
  
  String_free(&header);

  Jpeg_buffer_discard(jpeg_in);
}


static void Wav_handler(Instance *pi, void *data)
{
  MjxRepair_private *priv = (MjxRepair_private *)pi;  
  Wav_buffer *wav_in = data;
  Log(LOG_WAV, "%s popped wav_in @ %p", __func__, wav_in);

  /* Format header. */
  String *header = String_sprintf(part_format,
				  BOUNDARY,
				  "audio/x-wav",
				  wav_in->tv.tv_sec, wav_in->tv.tv_usec,
				  "",
				  wav_in->header_length+wav_in->data_length);


  if (priv->sink) {
    Sink_write(priv->sink, header->bytes, header->len);
    Sink_write(priv->sink, wav_in->header, wav_in->header_length);
    Sink_write(priv->sink, wav_in->data, wav_in->data_length);
  }

  String_free(&header);
  Wav_buffer_discard(&wav_in);
}


static void MjxRepair_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void MjxRepair_instance_init(Instance *pi)
{
}


static Template MjxRepair_template = {
  .label = "MjxRepair",
  .priv_size = sizeof(MjxRepair_private),
  .inputs = MjxRepair_inputs,
  .num_inputs = table_size(MjxRepair_inputs),
  .outputs = MjxRepair_outputs,
  .num_outputs = table_size(MjxRepair_outputs),
  .tick = MjxRepair_tick,
  .instance_init = MjxRepair_instance_init,
};

void MjxRepair_init(void)
{
  Template_register(&MjxRepair_template);
}
