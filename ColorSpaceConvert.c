/*
 * This module uses the existing colorspace conversions in Images.c.
 * The initial use is on the RPi2, sitting between DJpeg and the
 * RPiH264Enc and do that the 422->420 conversion doesn't happen
 * inline with the jpeg decode, which can push decode time past the
 * frame period time. As it develops, I'd like this module to
 * automatically convert any input image to all connected outputs.
 */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "ColorSpaceConvert.h"
#include "Images.h"

static void y422p_handler(Instance *pi, void *msg);
static void y420p_handler(Instance *pi, void *msg);
static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV422P, INPUT_YUV420P };
static Input ColorSpaceConvert_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = y420p_handler },
};

enum { OUTPUT_YUV422P, OUTPUT_YUV420P, OUTPUT_GRAY };
static Output ColorSpaceConvert_outputs[] = {
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = { .type_label = "GRAY_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
} ColorSpaceConvert_private;

static void y422p_handler(Instance *pi, void *msg)
{
  // ColorSpaceConvert_private *priv = (ColorSpaceConvert_private *)pi;
  YUV422P_buffer * y422p = msg;

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    YUV420P_buffer * y420p = YUV422P_to_YUV420P(y422p);
    PostData(y420p, pi->outputs[OUTPUT_YUV420P].destination);
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    Gray_buffer * gray = YUV422P_to_Gray(y422p);
    PostData(gray, pi->outputs[OUTPUT_GRAY].destination);
  }

  if (pi->outputs[OUTPUT_YUV422P].destination) {
    /* Pass-thru */
    PostData(y422p, pi->outputs[OUTPUT_YUV422P].destination);
  }
  else {
    YUV422P_buffer_release(y422p);
  }
}

static void y420p_handler(Instance *pi, void *msg)
{
  // ColorSpaceConvert_private *priv = (ColorSpaceConvert_private *)pi;
  YUV420P_buffer * y420p = msg;

  if (pi->outputs[OUTPUT_YUV422P].destination) {
    YUV422P_buffer * y422p = YUV420P_to_YUV422P(y420p);
    PostData(y422p, pi->outputs[OUTPUT_YUV422P].destination);
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    Gray_buffer * gray = YUV420P_to_Gray(y420p);
    PostData(gray, pi->outputs[OUTPUT_GRAY].destination);
  }

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    /* Pass-thru */
    PostData(y420p, pi->outputs[OUTPUT_YUV420P].destination);
  }
  else {
    YUV420P_buffer_release(y420p);
  }
}

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void ColorSpaceConvert_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void ColorSpaceConvert_instance_init(Instance *pi)
{
  // ColorSpaceConvert_private *priv = (ColorSpaceConvert_private *)pi;
}


static Template ColorSpaceConvert_template = {
  .label = "ColorSpaceConvert",
  .priv_size = sizeof(ColorSpaceConvert_private),
  .inputs = ColorSpaceConvert_inputs,
  .num_inputs = table_size(ColorSpaceConvert_inputs),
  .outputs = ColorSpaceConvert_outputs,
  .num_outputs = table_size(ColorSpaceConvert_outputs),
  .tick = ColorSpaceConvert_tick,
  .instance_init = ColorSpaceConvert_instance_init,
};

void ColorSpaceConvert_init(void)
{
  Template_register(&ColorSpaceConvert_template);
}
