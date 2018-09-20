/*
 * This module is incomplete.
 */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "ATSCTuner.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ATSCTuner_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output ATSCTuner_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String * frontend_path;
  String * demux_path;
  String * channel;
} ATSCTuner_private;

static Config config_table[] = {
  //{ "frontend_path",  set_frontend_path, 0L, 0L },
  //{ "demux_path",  set_demux_path, 0L, 0L },
  //{ "channel",  set_channel, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void ATSCTuner_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void ATSCTuner_instance_init(Instance *pi)
{
  // ATSCTuner_private *priv = (ATSCTuner_private *)pi;
}


static Template ATSCTuner_template = {
  .label = "ATSCTuner",
  .priv_size = sizeof(ATSCTuner_private),
  .inputs = ATSCTuner_inputs,
  .num_inputs = table_size(ATSCTuner_inputs),
  .outputs = ATSCTuner_outputs,
  .num_outputs = table_size(ATSCTuner_outputs),
  .tick = ATSCTuner_tick,
  .instance_init = ATSCTuner_instance_init,
};

void ATSCTuner_init(void)
{
  Template_register(&ATSCTuner_template);
}
