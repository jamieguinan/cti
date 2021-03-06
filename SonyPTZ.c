/* Sony Pan/Tilt/Zoom control over serial port. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "SonyPTZ.h"
#include "libvisca/libvisca.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input SonyPTZ_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output SonyPTZ_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  VISCAInterface_t interface;
  char *device;
} SonyPTZ_private;


static int set_device(Instance *pi, const char *value)
{
  SonyPTZ_private *priv = (SonyPTZ_private *)pi;
  unsigned int rc;

  priv->device = strdup(value);
  rc = VISCA_open_serial(&priv->interface, priv->device);

  return rc;
}

static Config config_table[] = {
  { "device", set_device, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void SonyPTZ_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void SonyPTZ_instance_init(Instance *pi)
{
  // SonyPTZ_private *priv = (SonyPTZ_private *)pi;
}


static Template SonyPTZ_template = {
  .label = "SonyPTZ",
  .priv_size = sizeof(SonyPTZ_private),
  .inputs = SonyPTZ_inputs,
  .num_inputs = table_size(SonyPTZ_inputs),
  .outputs = SonyPTZ_outputs,
  .num_outputs = table_size(SonyPTZ_outputs),
  .tick = SonyPTZ_tick,
  .instance_init = SonyPTZ_instance_init,
};

void SonyPTZ_init(void)
{
  Template_register(&SonyPTZ_template);
}
