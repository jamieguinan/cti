/* Send input to connected outputs in rotating order. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "Alternator.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input Alternator_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

enum { OUTPUT_JPEG_1, OUTPUT_JPEG_2, OUTPUT_JPEG_3 };
static Output Alternator_outputs[] = {
  [ OUTPUT_JPEG_1 ] = {.type_label = "Jpeg_buffer.1", .destination = 0L },
  [ OUTPUT_JPEG_2 ] = {.type_label = "Jpeg_buffer.2", .destination = 0L },
  [ OUTPUT_JPEG_3 ] = {.type_label = "Jpeg_buffer.3", .destination = 0L },
};

typedef struct {
  Instance i;
  int jpeg_connected_index;
} Alternator_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

#define MAX_OUTPUTS 3
static void Jpeg_handler(Instance *pi, void *data)
{
  Alternator_private *priv = (Alternator_private *)pi;
  const int available_outputs[] = { OUTPUT_JPEG_1, OUTPUT_JPEG_2, OUTPUT_JPEG_3 };
  int connected_outputs[MAX_OUTPUTS];
  int connected_count = 0;
  int i, j;
  Jpeg_buffer *jpeg_in = data;

  for (i=0; i < MAX_OUTPUTS; i++) {
    j = available_outputs[i];
    if (pi->outputs[j].destination) {
      connected_outputs[i] = j;
      connected_count += 1;
    }
  }

  if (connected_count == 0) {
    /* No connected outputs. */
    Jpeg_buffer_release(jpeg_in);
    return;
  }

  /* connected_outputs[] now contains indexes to connected outputs */
  priv->jpeg_connected_index = (priv->jpeg_connected_index + 1) % connected_count;

  for (i=0; i < connected_count; i++) {
    if (i == priv->jpeg_connected_index) {
      j = connected_outputs[i];
      PostData(jpeg_in, pi->outputs[j].destination);
      break;
    }
  }
}


static void Alternator_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Alternator_instance_init(Instance *pi)
{
  // Alternator_private *priv = (Alternator_private *)pi;
}


static Template Alternator_template = {
  .label = "Alternator",
  .priv_size = sizeof(Alternator_private),
  .inputs = Alternator_inputs,
  .num_inputs = table_size(Alternator_inputs),
  .outputs = Alternator_outputs,
  .num_outputs = table_size(Alternator_outputs),
  .tick = Alternator_tick,
  .instance_init = Alternator_instance_init,
};

void Alternator_init(void)
{
  Template_register(&Alternator_template);
}
