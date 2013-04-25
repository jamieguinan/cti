#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "JpegFiler.h"
#include "Images.h"
#include "Cfg.h"
#include "CTI.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input JpegFiler_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

//enum { /* OUTPUT_... */ };
static Output JpegFiler_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} JpegFiler_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Jpeg_handler(Instance *pi, void *msg)
{
  Jpeg_buffer *jpeg_in = msg;

  if (pi->counter % 1 == 0) {
    char name[256];
    sprintf(name, "%09d.jpg", pi->counter);
    FILE *f = fopen(name, "wb");
    if (f) {
      if (fwrite(jpeg_in->data, jpeg_in->encoded_length, 1, f) != 1) {
	perror("fwrite");
      }
      fclose(f);
    }
  }

  /* Discard input buffer. */
  Jpeg_buffer_discard(jpeg_in);
  pi->counter += 1;
}


static void JpegFiler_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}


static void JpegFiler_instance_init(Instance *pi)
{
}

static Template JpegFiler_template = {
  .label = "JpegFiler",
  .inputs = JpegFiler_inputs,
  .num_inputs = table_size(JpegFiler_inputs),
  .outputs = JpegFiler_outputs,
  .num_outputs = table_size(JpegFiler_outputs),
  .tick = JpegFiler_tick,
  .instance_init = JpegFiler_instance_init,
};

void JpegFiler_init(void)
{
  Template_register(&JpegFiler_template);
}
