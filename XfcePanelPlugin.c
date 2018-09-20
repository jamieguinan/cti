#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "XfcePanelPlugin.h"
#include <libxfce4panel/xfce-panel-plugin.h>


static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input XfcePanelPlugin_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output XfcePanelPlugin_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} XfcePanelPlugin_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void XfcePanelPlugin_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void XfcePanelPlugin_instance_init(Instance *pi)
{
  XfcePanelPlugin_private *priv = (XfcePanelPlugin_private *)pi;

}


static Template XfcePanelPlugin_template = {
  .label = "XfcePanelPlugin",
  .priv_size = sizeof(XfcePanelPlugin_private),
  .inputs = XfcePanelPlugin_inputs,
  .num_inputs = table_size(XfcePanelPlugin_inputs),
  .outputs = XfcePanelPlugin_outputs,
  .num_outputs = table_size(XfcePanelPlugin_outputs),
  .tick = XfcePanelPlugin_tick,
  .instance_init = XfcePanelPlugin_instance_init,
};

void XfcePanelPlugin_init(void)
{
  Template_register(&XfcePanelPlugin_template);
}
