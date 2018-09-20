/* Search and replace "Example" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Example.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input Example_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output Example_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} Example_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Example_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Example_instance_init(Instance *pi)
{
  Example_private *priv = (Example_private *)pi;
}


static Template Example_template = {
  .label = "Example",
  .priv_size = sizeof(Example_private),
  .inputs = Example_inputs,
  .num_inputs = table_size(Example_inputs),
  .outputs = Example_outputs,
  .num_outputs = table_size(Example_outputs),
  .tick = Example_tick,
  .instance_init = Example_instance_init,
};

void Example_init(void)
{
  Template_register(&Example_template);
}
