/* Search and replace "RPiH264Enc" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "RPiH264Enc.h"

static void Config_handler(Instance *pi, void *msg);
static void y420p_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV420P, INPUT_YUV422P };
static Input RPiH264Enc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = y420p_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

//enum { /* OUTPUT_... */ };
static Output RPiH264Enc_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} RPiH264Enc_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void y420p_handler(Instance *pi, void *msg)
{
}


static void y422p_handler(Instance *pi, void *msg)
{
}


static void RPiH264Enc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void RPiH264Enc_instance_init(Instance *pi)
{
  // RPiH264Enc_private *priv = (RPiH264Enc_private *)pi;
}


static Template RPiH264Enc_template = {
  .label = "RPiH264Enc",
  .priv_size = sizeof(RPiH264Enc_private),  
  .inputs = RPiH264Enc_inputs,
  .num_inputs = table_size(RPiH264Enc_inputs),
  .outputs = RPiH264Enc_outputs,
  .num_outputs = table_size(RPiH264Enc_outputs),
  .tick = RPiH264Enc_tick,
  .instance_init = RPiH264Enc_instance_init,
};

void RPiH264Enc_init(void)
{
  Template_register(&RPiH264Enc_template);
}
