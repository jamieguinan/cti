#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "Images.h"
#include "CTI.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);


enum { INPUT_CONFIG, INPUT_YUV422P };
static Input HalfWidth_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_YUV422P /* , OUTPUT_JPEG  */};
static Output HalfWidth_outputs[] = {
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
} HalfWidth_private;

static Config config_table[] = {
};

static void half_width_scale(uint8_t *indata, int width, int height, uint8_t *outdata)
{
  int i;
  for (i=0; i < (width*height)/2; i++) {
    outdata[0] = (indata[0] + indata[1])/2;
    indata += 2;
    outdata += 1;
  }
}

static void y422p_handler(Instance *pi, void *data)
{
  YUV422P_buffer *y422p_in = data;
  if (pi->outputs[OUTPUT_YUV422P].destination) {
    YUV422P_buffer *y422p_out = YUV422P_buffer_new(y422p_in->width/2, y422p_in->height, &y422p_in->c);
    half_width_scale(y422p_in->y, y422p_in->width, y422p_in->height, y422p_out->y);
    half_width_scale(y422p_in->cr, y422p_in->width/2, y422p_in->height, y422p_out->cr);
    half_width_scale(y422p_in->cb, y422p_in->width/2, y422p_in->height, y422p_out->cb);
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


static void HalfWidth_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void HalfWidth_instance_init(Instance *pi)
{
  // HalfWidth_private *priv = (HalfWidth_private *)pi;
}


static Template HalfWidth_template = {
  .label = "HalfWidth",
  .priv_size = sizeof(HalfWidth_private),
  .inputs = HalfWidth_inputs,
  .num_inputs = table_size(HalfWidth_inputs),
  .outputs = HalfWidth_outputs,
  .num_outputs = table_size(HalfWidth_outputs),
  .tick = HalfWidth_tick,
  .instance_init = HalfWidth_instance_init,
};

void HalfWidth_init(void)
{
  Template_register(&HalfWidth_template);
}


