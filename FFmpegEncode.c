/* Search and replace "FFmpeg" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input FFmpeg_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

//enum { /* OUTPUT_... */ };
static Output FFmpeg_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  FILE *p;
} FFmpeg_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Jpeg_handler(Instance *pi, void *msg)
{
  FFmpeg_private *priv = pi->data;

  if (!priv->p) {
    priv->p = popen("ffmpeg -i /dev/stdin -y out.h264", "wb");
  }
}

static void FFmpeg_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void FFmpeg_instance_init(Instance *pi)
{
  FFmpeg_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template FFmpeg_template = {
  .label = "FFmpeg",
  .inputs = FFmpeg_inputs,
  .num_inputs = table_size(FFmpeg_inputs),
  .outputs = FFmpeg_outputs,
  .num_outputs = table_size(FFmpeg_outputs),
  .tick = FFmpeg_tick,
  .instance_init = FFmpeg_instance_init,
};

void FFmpeg_init(void)
{
  Template_register(&FFmpeg_template);
}
