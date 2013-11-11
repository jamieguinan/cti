/* Search and replace "SubProc" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "SubProc.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);
static void RawData_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_RAWDATA };
static Input SubProc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
};

//enum { /* OUTPUT_... */ };
static Output SubProc_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  Sink *sp;
  String *cmd;
} SubProc_private;


static int set_proc(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;
  if (priv->sp) {
    Sink_close_current(priv->sp);
    Sink_free(&priv->sp);
  }

  if (priv->cmd) {
    String_free(&priv->cmd);
  }

  printf("set_proc(%s)\n", value);
  priv->cmd = String_sprintf("%s|", value);
  priv->sp = Sink_new(s(priv->cmd));
  Sink_write(priv->sp, "ch2\n", strlen("ch2\n"));
  Sink_flush(priv->sp);
  
  return 0;
}

static Config config_table[] = {
  { "proc", set_proc, NULL, NULL},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void RawData_handler(Instance *pi, void *data)
{
  
}


static void SubProc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void SubProc_instance_init(Instance *pi)
{
  // SubProc_private *priv = (SubProc_private *)pi;
}


static Template SubProc_template = {
  .label = "SubProc",
  .priv_size = sizeof(SubProc_private),  
  .inputs = SubProc_inputs,
  .num_inputs = table_size(SubProc_inputs),
  .outputs = SubProc_outputs,
  .num_outputs = table_size(SubProc_outputs),
  .tick = SubProc_tick,
  .instance_init = SubProc_instance_init,
};

void SubProc_init(void)
{
  Template_register(&SubProc_template);
}
