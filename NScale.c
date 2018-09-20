/*
 * Scale image width and/or height by 1/N.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "NScale.h"
#include "Images.h"
#include "CTI.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);


enum { INPUT_CONFIG, INPUT_YUV422P };
static Input NScale_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_YUV422P /* , OUTPUT_JPEG  */};
static Output NScale_outputs[] = {
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  int Nx;
  int Ny;
} NScale_private;


static Config config_table[] = {
  { "Nx",  0L, 0L, 0L, cti_set_int, offsetof(NScale_private, Nx) },
  { "Ny",  0L, 0L, 0L, cti_set_int, offsetof(NScale_private, Ny) },
};

static void N_scale(NScale_private *priv, uint8_t *indata, int in_width, int in_height,
		    uint8_t *outdata, int out_width, int out_height)
{
  int i, j, k;
  int x, y;
  int count = priv->Nx * priv->Ny;
  uint16_t tmp_out[out_width];

  for (y=0; y < out_height; y++) {
    k = 0;
    memset(tmp_out, 0, sizeof(tmp_out));
    for (j=0; j < priv->Ny; j++) { /* For each input row */
      k = 0;
      for (x=0; x < out_width; x++) { /* For each temporary output pixel sum */
	for (i=0; i < priv->Nx; i++) { /* For each group of horizontal pixels */
	  tmp_out[k] += *indata++;
	}
	k++;
      }
    }
    for (x=0; x < out_width; x++) { /* Divide and copy out the temporary output pixel sums */
      outdata[x] = tmp_out[x]/count;
    }
  }
}

static void y422p_handler(Instance *pi, void *data)
{
  NScale_private *priv = (NScale_private *)pi;
  YUV422P_buffer *y422p_in = data;

  if (pi->outputs[OUTPUT_YUV422P].destination) {

    if (priv->Nx == 1 && priv->Ny == 1) {
      /* Pass-through */
      PostData(y422p_in, pi->outputs[OUTPUT_YUV422P].destination);
      return;
    }

    /* else... */

    YUV422P_buffer *y422p_out = YUV422P_buffer_new(y422p_in->width/priv->Nx, y422p_in->height/priv->Ny,
					       &y422p_in->c);
    N_scale(priv, y422p_in->y, y422p_in->width, y422p_in->height,
	    y422p_out->y, y422p_out->width, y422p_out->height);
    N_scale(priv, y422p_in->cr, y422p_in->cr_width, y422p_in->cr_height,
	    y422p_out->cr, y422p_out->cr_width, y422p_out->cr_height);
    N_scale(priv, y422p_in->cb, y422p_in->cb_width, y422p_in->cb_height,
	    y422p_out->cb, y422p_out->cb_width, y422p_out->cb_height);
    dpf("posting %dx%d image to output\n", y422p_out->width, y422p_out->height);
    y422p_out->c.timestamp = y422p_in->c.timestamp;
    PostData(y422p_out, pi->outputs[OUTPUT_YUV422P].destination);
  }
  YUV422P_buffer_release(y422p_in);
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void NScale_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void NScale_instance_init(Instance *pi)
{
  NScale_private *priv = (NScale_private *)pi;

  priv->Nx = 1;
  priv->Ny = 1;
}


static Template NScale_template = {
  .label = "NScale",
  .priv_size = sizeof(NScale_private),
  .inputs = NScale_inputs,
  .num_inputs = table_size(NScale_inputs),
  .outputs = NScale_outputs,
  .num_outputs = table_size(NScale_outputs),
  .tick = NScale_tick,
  .instance_init = NScale_instance_init,
};

void NScale_init(void)
{
  Template_register(&NScale_template);
}


