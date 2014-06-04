/* HTTP Server module. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "HTTPServer.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input HTTPServer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output HTTPServer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} HTTPServer_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void HTTPServer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void HTTPServer_instance_init(Instance *pi)
{
  HTTPServer_private *priv = (HTTPServer_private *)pi;
}


static Template HTTPServer_template = {
  .label = "HTTPServer",
  .priv_size = sizeof(HTTPServer_private),  
  .inputs = HTTPServer_inputs,
  .num_inputs = table_size(HTTPServer_inputs),
  .outputs = HTTPServer_outputs,
  .num_outputs = table_size(HTTPServer_outputs),
  .tick = HTTPServer_tick,
  .instance_init = HTTPServer_instance_init,
};

void HTTPServer_init(void)
{
  Template_register(&HTTPServer_template);
}
