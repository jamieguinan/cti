/* 
 * Dynamically control a V4L2Capture instance.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "DynamicVideoControl.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input DynamicVideoControl_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output DynamicVideoControl_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  //Gray_buffer * ...;
} DynamicVideoControl_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void DynamicVideoControl_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void DynamicVideoControl_instance_init(Instance *pi)
{
  DynamicVideoControl_private *priv = (DynamicVideoControl_private *)pi;
}


static Template DynamicVideoControl_template = {
  .label = "DynamicVideoControl",
  .priv_size = sizeof(DynamicVideoControl_private),  
  .inputs = DynamicVideoControl_inputs,
  .num_inputs = table_size(DynamicVideoControl_inputs),
  .outputs = DynamicVideoControl_outputs,
  .num_outputs = table_size(DynamicVideoControl_outputs),
  .tick = DynamicVideoControl_tick,
  .instance_init = DynamicVideoControl_instance_init,
};

void DynamicVideoControl_init(void)
{
  Template_register(&DynamicVideoControl_template);
}
