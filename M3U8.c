/* Search and replace "M3U8" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "M3U8.h"
#include "SourceSink.h"
#include "HTTPClient.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input M3U8_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output M3U8_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


typedef struct {
  Instance i;
  String * url;
  HTTPTransaction * t;
  Comm * stream;
} M3U8_private;


static void M3U8_reset(M3U8_private *priv)
{
  if (priv->stream) {
    Comm_free(&(priv->stream));
  }
  if (priv->t) {
    HTTP_clear(&priv->t);
  }
}


static void handle_http_complete(M3U8_private *priv)
{
}


static int set_url(Instance *pi, const char *value)
{
  M3U8_private *priv = (M3U8_private *)pi;
  M3U8_reset(priv);
  String_set(&priv->url, value);
  HTTP_init_get(&priv->t, priv->url);
  return 0;
}


static Config config_table[] = {
  { "url", set_url, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void M3U8_tick(Instance *pi)
{
  M3U8_private *priv = (M3U8_private *)pi;
  Handler_message *hm;
  int wait_flag = 1;

  if (priv->t && HTTP_state(priv->t) == HTTP_STATE_ACTIVE) {
    HTTP_process(priv->t);
    if (HTTP_state(priv->t) == HTTP_STATE_COMPLETE) {
      handle_http_complete(priv);
    }
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void M3U8_instance_init(Instance *pi)
{
  M3U8_private *priv = (M3U8_private *)pi;
  priv->url = String_value_none();
}


static Template M3U8_template = {
  .label = "M3U8",
  .priv_size = sizeof(M3U8_private),  
  .inputs = M3U8_inputs,
  .num_inputs = table_size(M3U8_inputs),
  .outputs = M3U8_outputs,
  .num_outputs = table_size(M3U8_outputs),
  .tick = M3U8_tick,
  .instance_init = M3U8_instance_init,
};

void M3U8_init(void)
{
  Template_register(&M3U8_template);
}
