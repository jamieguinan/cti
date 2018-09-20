/* Tap/tee various kinds of messages. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Tap.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input Tap_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

enum { OUTPUT_JPEG_1, OUTPUT_JPEG_2 };

static Output Tap_outputs[] = {
  [ OUTPUT_JPEG_1 ] = { .type_label = "Jpeg_buffer.1", .destination = 0L },
  [ OUTPUT_JPEG_2 ] = { .type_label = "Jpeg_buffer.2", .destination = 0L },
};

typedef struct {
  Instance i;
} Tap_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Jpeg_handler(Instance *pi, void *msg)
{
  Jpeg_buffer *jpeg = msg;

  if (pi->outputs[OUTPUT_JPEG_1].destination && pi->outputs[OUTPUT_JPEG_2].destination) {
    /* Clone buffer, post to both. */
    Jpeg_buffer *clone = Mem_malloc(sizeof(*clone));
    memcpy(clone, jpeg, sizeof(*jpeg));
    clone->data = Mem_malloc(clone->encoded_length); /* Reset data to new block. */
    memcpy(clone->data, jpeg->data, clone->encoded_length); /* Copy. */
    PostData(jpeg, pi->outputs[OUTPUT_JPEG_1].destination);
    PostData(clone, pi->outputs[OUTPUT_JPEG_2].destination);
  }
  else if (pi->outputs[OUTPUT_JPEG_1].destination) {
    /* Pass along. */
    PostData(jpeg, pi->outputs[OUTPUT_JPEG_1].destination);
  }
  else if (pi->outputs[OUTPUT_JPEG_2].destination) {
    /* Pass along. */
    PostData(jpeg, pi->outputs[OUTPUT_JPEG_2].destination);
  }
  else {
    /* No output, discard buffer! */
    Jpeg_buffer_release(jpeg);
  }
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Tap_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Tap_instance_init(Instance *pi)
{
  // Tap_private *priv = (Tap_private *)pi;
}


static Template Tap_template = {
  .label = "Tap",
  .priv_size = sizeof(Tap_private),
  .inputs = Tap_inputs,
  .num_inputs = table_size(Tap_inputs),
  .outputs = Tap_outputs,
  .num_outputs = table_size(Tap_outputs),
  .tick = Tap_tick,
  .instance_init = Tap_instance_init,
};

void Tap_init(void)
{
  Template_register(&Tap_template);
}
