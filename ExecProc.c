/*
 * Sub-process module, which run sub-programs without keeping track of them.
 * This was started as a copy of SubProc.c, but intended to be a simpler
 * open-loop handler.
 */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc, system */
#include <string.h>             /* memcpy */

#include "CTI.h"
#include "ExecProc.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ExecProc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_CONFIG };
static Output ExecProc_outputs[] = {
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  /* The sub-process command to run. */
  String *proc;
} ExecProc_private;


static int set_proc(Instance *pi, const char *value)
{
  ExecProc_private *priv = (ExecProc_private *)pi;

  String_free(&priv->proc);

  fprintf(stderr, "%s(%s)\n", __func__, value);
  priv->proc = String_new(value);

  return 0;
}


static int handle_token(Instance *pi, const char *value)
{
  ExecProc_private *priv = (ExecProc_private *)pi;
  char cmd[strlen(value) + String_len(priv->proc) + 32];
  sprintf(cmd, "%s %s", s(priv->proc), value);

  // fprintf(stderr, "%s: %s\n", __func__, cmd);
  system(cmd);
  return 0;
}


static Config config_table[] = {
  { "proc", set_proc, NULL, NULL},
  { "token", handle_token, NULL, NULL},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void ExecProc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void ExecProc_instance_init(Instance *pi)
{
  ExecProc_private *priv = (ExecProc_private *)pi;
  priv->proc = String_new("true");
}


static Template ExecProc_template = {
  .label = "ExecProc",
  .priv_size = sizeof(ExecProc_private),
  .inputs = ExecProc_inputs,
  .num_inputs = table_size(ExecProc_inputs),
  .outputs = ExecProc_outputs,
  .num_outputs = table_size(ExecProc_outputs),
  .tick = ExecProc_tick,
  .instance_init = ExecProc_instance_init,
};

void ExecProc_init(void)
{
  Template_register(&ExecProc_template);
}
