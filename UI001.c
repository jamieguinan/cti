#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "UI001.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input UI001_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output UI001_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  // int ...;
} UI001_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void UI001_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void UI001_instance_init(Instance *pi)
{
  UI001_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template UI001_template = {
  .label = "UI001",
  .inputs = UI001_inputs,
  .num_inputs = table_size(UI001_inputs),
  .outputs = UI001_outputs,
  .num_outputs = table_size(UI001_outputs),
  .tick = UI001_tick,
  .instance_init = UI001_instance_init,
};

void UI001_init(void)
{
  Template_register(&UI001_template);
}
