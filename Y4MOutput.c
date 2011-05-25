/* YUV4MPEG2 output streaming. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "SourceSink.h"
#include "Images.h"
#include "Y4MOutput.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422P_handler(Instance *pi, void *data);
static void RGB3_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_422P, INPUT_RGB3, INPUT_BGR3 };
static Input Y4MOutput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
};

static Output Y4MOutput_outputs[] = {
};

enum { Y4MOUTPUT_STATE_INIT, Y4MOUTPUT_STATE_HEADER_SENT, Y4MOUTPUT_STATE_SENDING_DATA };

typedef struct {
  char *output;		/* File or host:port, used to intialize sink. */
  Sink *sink;
  int state;
  int width;
  int height;
  int fps_nom;
  int fps_denom;
  int use_420;
} Y4MOutput_private;


static int set_output(Instance *pi, const char *value)
{
  Y4MOutput_private *priv = pi->data;

  if (priv->output) {
    free(priv->output);
  }
  if (priv->sink) {
    Sink_free(&priv->sink);
  }

  priv->output = strdup(value);
  priv->sink = Sink_new(priv->output);

  return 0;
}


static int set_fps_nom(Instance *pi, const char *value)
{
  Y4MOutput_private *priv = pi->data;
  priv->fps_nom = atoi(value);
  return 0;
}


static int set_fps_denom(Instance *pi, const char *value)
{
  Y4MOutput_private *priv = pi->data;
  priv->fps_denom = atoi(value);
  return 0;
}


static Config config_table[] = {
  { "output", set_output , 0L, 0L },
  { "fps_nom", set_fps_nom , 0L, 0L },
  { "fps_denom", set_fps_denom , 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Y422P_handler(Instance *pi, void *data)
{
  Y4MOutput_private *priv = pi->data;
  Y422P_buffer *y422p_in = data;
  Y420P_buffer *y420p = 0L;
  char header[256];

  if (!priv->sink) {
    goto out;
  }

  if (!priv->use_420) {
    /* Preferred, Y422P. */
    if (priv->state == Y4MOUTPUT_STATE_INIT) {
      priv->width = y422p_in->width;
      priv->height = y422p_in->height;
      snprintf(header, sizeof(header)-1, "YUV4MPEG2 W%d H%d C422 Ip F%d:%d A1:1\n",
	       priv->width,
	       priv->height,
	       priv->fps_nom,
	       priv->fps_denom);
      Sink_write(priv->sink, header, strlen(header));
      priv->state = Y4MOUTPUT_STATE_SENDING_DATA;
    }
    
    snprintf(header, sizeof(header)-1, "FRAME\n");
    Sink_write(priv->sink, header, strlen(header));
    
    Sink_write(priv->sink, y422p_in->y, y422p_in->y_length);
    Sink_write(priv->sink, y422p_in->cb, y422p_in->cb_length);
    Sink_write(priv->sink, y422p_in->cr, y422p_in->cr_length);
  }
  else {
    /* For downstream that can only handle Y420P. */
    y420p = Y422P_to_Y420P(y422p_in);

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

    snprintf(header, sizeof(header)-1, "FRAME\n");
    Sink_write(priv->sink, header, strlen(header));
  
    Sink_write(priv->sink, y420p->y, y420p->y_length);
    Sink_write(priv->sink, y420p->cb, y420p->cb_length);
    Sink_write(priv->sink, y420p->cr, y420p->cr_length);
  }

 out:
  Y422P_buffer_discard(y422p_in);
  if (y420p) {
    Y420P_buffer_discard(y420p);
  }
}

static void RGB3_handler(Instance *pi, void *data)
{
  RGB3_buffer *rgb3_in = data;
  Y422P_buffer *y4m = RGB3_toY422P(rgb3_in);

  Y422P_handler(pi, y4m);
  RGB3_buffer_discard(rgb3_in);
}

static void BGR3_handler(Instance *pi, void *data)
{
  BGR3_buffer *bgr3_in = data;
  Y422P_buffer *y4m = BGR3_toY422P(bgr3_in);

  Y422P_handler(pi, y4m);
  BGR3_buffer_discard(bgr3_in);
}

static void Y4MOutput_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void Y4MOutput_instance_init(Instance *pi)
{
  Y4MOutput_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template Y4MOutput_template = {
  .label = "Y4MOutput",
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
