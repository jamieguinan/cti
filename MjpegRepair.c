/* Search and replace "MjpegRepair" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "MjpegRepair.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input MjpegRepair_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output MjpegRepair_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} MjpegRepair_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void MjpegRepair_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void MjpegRepair_instance_init(Instance *pi)
{
  MjpegRepair_private *priv = (MjpegRepair_private *)pi;

}


static Template MjpegRepair_template = {
  .label = "MjpegRepair",
  .priv_size = sizeof(MjpegRepair_private),
  .inputs = MjpegRepair_inputs,
  .num_inputs = table_size(MjpegRepair_inputs),
  .outputs = MjpegRepair_outputs,
  .num_outputs = table_size(MjpegRepair_outputs),
  .tick = MjpegRepair_tick,
  .instance_init = MjpegRepair_instance_init,
};

void MjpegRepair_init(void)
{
  Template_register(&MjpegRepair_template);
}
