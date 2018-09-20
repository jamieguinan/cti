/*
 * This is a simple relay between remote sockets and usually a local
 * SocketServer instance.  It can be enabled by a SocketServer
 * instance when client count is non-zero, and disabled when client
 * count transitions to zero.
 */

#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "SocketRelay.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input SocketRelay_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_RAWDATA };
static Output SocketRelay_outputs[] = {
  [ OUTPUT_RAWDATA ] =  { .type_label = "RawData_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  Source * source;
  int enable;
} SocketRelay_private;


static int set_input(Instance *pi, const char *value)
{
  SocketRelay_private *priv = (SocketRelay_private *)pi;

  if (priv->source) {
    Source_free(&priv->source);
  }

  priv->source = Source_allocate(value);
  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  SocketRelay_private *priv = (SocketRelay_private *)pi;
  int newval = atoi(value);
  if (newval == priv->enable) {
    return 1;
  }
  priv->enable = newval;

  if (priv->enable) {
    if (priv->source) {
      Source_reopen(priv->source);
    }
    else {
      fprintf(stderr, "%s/%s: source is not set\n", __FILE__, __func__);
      priv->enable = 0;
    }
  }

  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void SocketRelay_tick(Instance *pi)
{
  SocketRelay_private *priv = (SocketRelay_private *)pi;
  Handler_message *hm;
  int wait_flag = (priv->enable ? 0 : 1);

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (priv->enable && priv->source) {
    ArrayU8 * chunk = Source_read(priv->source);
    if (pi->outputs[OUTPUT_RAWDATA].destination) {
      RawData_buffer *raw = RawData_buffer_new(chunk->len);
      memcpy(raw->data, chunk->data, chunk->len);
      PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
    }
    ArrayU8_cleanup(&chunk);
  }

  pi->counter++;
}

static void SocketRelay_instance_init(Instance *pi)
{
  // SocketRelay_private *priv = (SocketRelay_private *)pi;
}


static Template SocketRelay_template = {
  .label = "SocketRelay",
  .priv_size = sizeof(SocketRelay_private),
  .inputs = SocketRelay_inputs,
  .num_inputs = table_size(SocketRelay_inputs),
  .outputs = SocketRelay_outputs,
  .num_outputs = table_size(SocketRelay_outputs),
  .tick = SocketRelay_tick,
  .instance_init = SocketRelay_instance_init,
};

void SocketRelay_init(void)
{
  Template_register(&SocketRelay_template);
}
