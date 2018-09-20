/*
 * See /tmp/libmpeg2-0.5.1/src/mpeg2dec.c for reference.
 */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "Mpeg2Dec.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input Mpeg2Dec_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output Mpeg2Dec_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} Mpeg2Dec_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Mpeg2Dec_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Mpeg2Dec_instance_init(Instance *pi)
{
  //Mpeg2Dec_private *priv = (Mpeg2Dec_private *)pi;
}


static Template Mpeg2Dec_template = {
  .label = "Mpeg2Dec",
  .priv_size = sizeof(Mpeg2Dec_private),
  .inputs = Mpeg2Dec_inputs,
  .num_inputs = table_size(Mpeg2Dec_inputs),
  .outputs = Mpeg2Dec_outputs,
  .num_outputs = table_size(Mpeg2Dec_outputs),
  .tick = Mpeg2Dec_tick,
  .instance_init = Mpeg2Dec_instance_init,
};

void Mpeg2Dec_init(void)
{
  Template_register(&Mpeg2Dec_template);
}


