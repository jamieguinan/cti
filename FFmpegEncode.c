/* Search and replace "FFmpeg" with new module name. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input FFmpegEncode_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

enum { OUTPUT_FEEDBACK };
static Output FFmpegEncode_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L  },
};

typedef struct {
  Instance i;
  FILE *p;
} FFmpegEncode_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Jpeg_handler(Instance *pi, void *msg)
{
  FFmpegEncode_private *priv = (FFmpegEncode_private *)pi;

  if (!priv->p) {
    priv->p = popen("ffmpeg -i /dev/stdin -y out.h264", "wb");
  }
}

static void FFmpegEncode_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void FFmpegEncode_instance_init(Instance *pi)
{
}


static Template FFmpegEncode_template = {
  .label = "FFmpegEncode",
  .inputs = FFmpegEncode_inputs,
  .num_inputs = table_size(FFmpegEncode_inputs),
  .outputs = FFmpegEncode_outputs,
  .num_outputs = table_size(FFmpegEncode_outputs),
  .tick = FFmpegEncode_tick,
  .instance_init = FFmpegEncode_instance_init,
};

void FFmpegEncode_init(void)
{
  Template_register(&FFmpegEncode_template);
}
