/* Search and replace "JpegFiler" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "JpegFiler.h"
#include "Images.h"
#include "Cfg.h"
#include "Template.h"

enum { INPUT_CONFIG, INPUT_JPEG };
static Input JpegFiler_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg" },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer" },
};

//enum { /* OUTPUT_... */ };
static Output JpegFiler_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  // int ...;
} JpegFiler_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void JpegFiler_tick(Instance *pi)
{
  Jpeg_buffer *jpeg_in;

  if (CheckMessage(pi, 1) == 0) {
    /* Weird, should only return positive... */
    return;
  }

  if (0)  {
    Config_buffer *cb_in;
    int i;

    cb_in = PopMessage(&pi->inputs[INPUT_CONFIG]);

    /* Walk the config table. */
    for (i=0; i < table_size(config_table); i++) {
      if (streq(config_table[i].label, cb_in->label->bytes)) {
	int rc;		/* FIXME: What to do with this? */
	rc = config_table[i].set(pi, cb_in->value->bytes);
	break;
      }
    }
    Config_buffer_discard(&cb_in);
  }

  if (0)  {
    /* Lock queue, remove first message. */
    jpeg_in = PopMessage(&pi->inputs[INPUT_JPEG]);
    if (cfg.verbosity) {
      printf("%s got message\n", __func__);
    }
  }
  else {
    /* Huh.  Nothing to do. */
    fprintf(stderr, "%s: nothing to do!\n", __func__);
    return;
  }
  
  if (pi->counter % 1 == 0) {
    char name[256];
    sprintf(name, "%09d.jpg", pi->counter);
    FILE *f = fopen(name, "wb");
    // fwrite(jpeg_in->data, jpeg_in->data_length, 1, f);
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


static void JpegFiler_instance_init(Instance *pi)
{
  JpegFiler_private *priv = calloc(1, sizeof(*priv));
  pi->data = priv;
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
