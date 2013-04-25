/* Search and replace "Splitter" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Splitter.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_422P };
static Input Splitter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_422P_1, OUTPUT_422P_2};
static Output Splitter_outputs[] = {
  [ OUTPUT_422P_1 ] = {.type_label = "422P_buffer.1", .destination = 0L },
  [ OUTPUT_422P_2 ] = {.type_label = "422P_buffer.2", .destination = 0L },
#define NUM_422P_OUTPUTS 2
};

typedef struct {
  Instance i;
  // int ...;
} Splitter_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void y422p_handler(Instance *pi, void *data)
{
  const int outputs[NUM_422P_OUTPUTS] = { OUTPUT_422P_1, OUTPUT_422P_2 };
  int i, j;
  int out_count = 0;
  Y422P_buffer *y422p_in = data;

  for (i=0; i < NUM_422P_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count += 1;
    }
  }

  for (i=0; out_count && i < NUM_422P_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count -= 1;
      if (out_count) {
	/* Clone and post buffer. */
	Y422P_buffer *y422p_tmp = Y422P_clone(y422p_in);
	PostData(y422p_tmp, pi->outputs[j].destination); 
	y422p_tmp = NULL;
      }
      else {
	/* Post buffer. */
	PostData(y422p_in, pi->outputs[j].destination); 
	y422p_in = 0L;
      }
    }
  }

  if (y422p_in) {
    /* There were no outputs. */
    Y422P_buffer_discard(y422p_in);
  }
}


static void Splitter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void Splitter_instance_init(Instance *pi)
{
  // Splitter_private *priv = (Splitter_private *)pi;
}


static Template Splitter_template = {
  .label = "Splitter",
  .inputs = Splitter_inputs,
  .num_inputs = table_size(Splitter_inputs),
  .outputs = Splitter_outputs,
  .num_outputs = table_size(Splitter_outputs),
  .tick = Splitter_tick,
  .instance_init = Splitter_instance_init,
};

void Splitter_init(void)
{
  Template_register(&Splitter_template);
}
