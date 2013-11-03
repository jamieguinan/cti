/* Write out images to files. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "ImageOutput.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422P_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_422P, INPUT_JPEG };
static Input ImageOutput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
};

//enum { /* OUTPUT_... */ };
static Output ImageOutput_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String output_base;
} ImageOutput_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Y422P_handler(Instance *pi, void *data)
{
  // ImageOutput_private * priv = (ImageOutput_private*) pi;
  Y422P_buffer *y422p_in = data;
  
  Y422P_buffer_discard(y422p_in);
}


static void ImageOutput_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void ImageOutput_instance_init(Instance *pi)
{
  ImageOutput_private *priv = (ImageOutput_private *)pi;
  String_set(&priv->output_base, "./");
}


static Template ImageOutput_template = {
  .label = "ImageOutput",
  .priv_size = sizeof(ImageOutput_private),  
  .inputs = ImageOutput_inputs,
  .num_inputs = table_size(ImageOutput_inputs),
  .outputs = ImageOutput_outputs,
  .num_outputs = table_size(ImageOutput_outputs),
  .tick = ImageOutput_tick,
  .instance_init = ImageOutput_instance_init,
};

void ImageOutput_init(void)
{
  Template_register(&ImageOutput_template);
}
