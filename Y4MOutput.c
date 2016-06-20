/* YUV4MPEG2 output streaming. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "SourceSink.h"
#include "Images.h"
#include "Y4MOutput.h"

static void Config_handler(Instance *pi, void *msg);
static void YUV422P_handler(Instance *pi, void *data);
static void YUV420P_handler(Instance *pi, void *data);
static void RGB3_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_YUV422P,  INPUT_YUV420P, INPUT_RGB3, INPUT_BGR3 };
static Input Y4MOutput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = YUV422P_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
};

static Output Y4MOutput_outputs[] = {
};

enum { Y4MOUTPUT_STATE_INIT, Y4MOUTPUT_STATE_HEADER_SENT, Y4MOUTPUT_STATE_SENDING_DATA };

typedef struct {
  Instance i;
  String interlace;
  Sink *sink;
  int state;
  int width;
  int height;
  int fps_nom;
  int fps_denom;
  int raw;
  int reduce;
} Y4MOutput_private;


static int set_output(Instance *pi, const char *value)
{
  Y4MOutput_private *priv = (Y4MOutput_private *)pi;

  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  priv->sink = Sink_new(value);

  return 0;
}


static Config config_table[] = {
  { "output", set_output, 0L, 0L },
  { "fps_nom", 0L, 0L, 0L, cti_set_int, offsetof(Y4MOutput_private, fps_nom) },
  { "fps_denom", 0L, 0L, 0L, cti_set_int, offsetof(Y4MOutput_private, fps_denom) },
  { "interlace", 0L, 0L, 0L, cti_set_string_local, offsetof(Y4MOutput_private, interlace) },
  { "raw", 0L, 0L, 0L, cti_set_int, offsetof(Y4MOutput_private, raw) },
  { "reduce", 0L, 0L, 0L, cti_set_int, offsetof(Y4MOutput_private, reduce) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void YUV422P_handler(Instance *pi, void *data)
{
  Y4MOutput_private *priv = (Y4MOutput_private *)pi;
  YUV422P_buffer *y422p_in = data;
  char header[256];

  if (y422p_in->c.eof) {
    Sink_close_current(priv->sink);
    priv->sink = NULL;
  }

  if (priv->reduce) {
    YUV420P_buffer *y420 = YUV422P_to_YUV420P(y422p_in);
    YUV420P_handler(pi, y420);
    goto out;
  }


  if (!priv->sink) {
    goto out;
  }

  if (!priv->raw) {
    if (priv->state == Y4MOUTPUT_STATE_INIT) {
      priv->width = y422p_in->width;
      priv->height = y422p_in->height;
      snprintf(header, sizeof(header)-1, "YUV4MPEG2 W%d H%d C422 I%s F%d:%d A1:1\n",
	       priv->width,
	       priv->height,
	       sl(priv->interlace),
	       priv->fps_nom,
	       priv->fps_denom);
      Sink_write(priv->sink, header, strlen(header));
      priv->state = Y4MOUTPUT_STATE_SENDING_DATA;
    }
  }

  if (!priv->raw) {
    snprintf(header, sizeof(header)-1, "FRAME\n");
    Sink_write(priv->sink, header, strlen(header));
  }
  
  Sink_write(priv->sink, y422p_in->y, y422p_in->y_length);
  Sink_write(priv->sink, y422p_in->cb, y422p_in->cb_length);
  Sink_write(priv->sink, y422p_in->cr, y422p_in->cr_length);

 out:
  YUV422P_buffer_release(y422p_in);
}


static void YUV420P_handler(Instance *pi, void *data)
{
  Y4MOutput_private *priv = (Y4MOutput_private *)pi;
  YUV420P_buffer *y420p = data;
  char header[256];

  if (y420p->c.eof) {
    Sink_close_current(priv->sink);
    priv->sink = NULL;
  }
  
  if (!priv->sink) {
    goto out;
  }
  
  if (!priv->raw) {
    if (priv->state == Y4MOUTPUT_STATE_INIT) {
      priv->width = y420p->width;
      priv->height = y420p->height;
      snprintf(header, sizeof(header)-1, "YUV4MPEG2 W%d H%d C420jpeg Ip F%d:%d A1:1\n",
	       priv->width,
	       priv->height,
	       priv->fps_nom,
	       priv->fps_denom);
      Sink_write(priv->sink, header, strlen(header));
      priv->state = Y4MOUTPUT_STATE_SENDING_DATA;
    }
  }

  if (!priv->raw) {  
    snprintf(header, sizeof(header)-1, "FRAME\n");
    Sink_write(priv->sink, header, strlen(header));
  }
  
  Sink_write(priv->sink, y420p->y, y420p->y_length);
  Sink_write(priv->sink, y420p->cb, y420p->cb_length);
  Sink_write(priv->sink, y420p->cr, y420p->cr_length);

 out:
  YUV420P_buffer_release(y420p);
}


static void RGB3_handler(Instance *pi, void *data)
{
  RGB3_buffer *rgb3_in = data;
  YUV422P_buffer *y4m = RGB3_to_YUV422P(rgb3_in);

  YUV422P_handler(pi, y4m);
  RGB3_buffer_release(rgb3_in);
}

static void BGR3_handler(Instance *pi, void *data)
{
  BGR3_buffer *bgr3_in = data;
  YUV422P_buffer *y4m = BGR3_toYUV422P(bgr3_in);

  YUV422P_handler(pi, y4m);
  BGR3_buffer_release(bgr3_in);
}

static void Y4MOutput_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Y4MOutput_instance_init(Instance *pi)
{
  Y4MOutput_private *priv = (Y4MOutput_private *)pi;
  String_set_local(&priv->interlace, "p");
}


static Template Y4MOutput_template = {
  .label = "Y4MOutput",
  .priv_size = sizeof(Y4MOutput_private),
  .inputs = Y4MOutput_inputs,
  .num_inputs = table_size(Y4MOutput_inputs),
  .outputs = Y4MOutput_outputs,
  .num_outputs = table_size(Y4MOutput_outputs),
  .tick = Y4MOutput_tick,
  .instance_init = Y4MOutput_instance_init,
};

void Y4MOutput_init(void)
{
  Template_register(&Y4MOutput_template);
}
