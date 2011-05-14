/* Search and replace "ALSAMixer" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "ALSAMixer.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ALSAMixer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output ALSAMixer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  // int ...;
} ALSAMixer_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

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

static void ALSAMixer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void ALSAMixer_instance_init(Instance *pi)
{
  ALSAMixer_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template ALSAMixer_template = {
  .label = "ALSAMixer",
  .inputs = ALSAMixer_inputs,
  .num_inputs = table_size(ALSAMixer_inputs),
  .outputs = ALSAMixer_outputs,
  .num_outputs = table_size(ALSAMixer_outputs),
  .tick = ALSAMixer_tick,
  .instance_init = ALSAMixer_instance_init,
};

void ALSAMixer_init(void)
{
  Template_register(&ALSAMixer_template);
}
