/* Sub-process module. */
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

enum { OUTPUT_CONFIG };
static Output SubProc_outputs[] = {
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;

  /* The subprocess invocation and handle is the main point of this
     module.  INPUT_RAWDATA is fed to stdin, and so are token messages
     unless control_channel is also set. */
  String *proc;
  Sink *proc_sp;

  /* But some programs might want separate control input, such as
     mplayer's slave mode.*/
  String *control_channel;
  Sink *control_sp;

  /* Here I'm thinking about capturing output, too.  Might have to add
   * a double-ended Pipe type to SourceSink module, or use Comm instead
   of a Sink and Source. */
  // String *return_channel;
  // Source *return_sp;

  /* Invocation for a cleanup command. */
  String *cleanup;
} SubProc_private;


static int set_proc(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;
  if (priv->proc_sp) {
    Sink_close_current(priv->proc_sp);
    Sink_free(&priv->proc_sp);
  }

  if (priv->proc) {
    String_free(&priv->proc);
  }

  fprintf(stderr, "%s(%s)\n", __func__, value);
  priv->proc = String_sprintf("|%s", value);
  
  return 0;
}


static int set_control_channel(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;
  if (priv->control_sp) {
    Sink_close_current(priv->control_sp);
    Sink_free(&priv->control_sp);
  }

  if (priv->control_channel) {
    // String_unref(&priv->control_channel);
  }

  fprintf(stderr, "%s(%s)\n", __func__, value);
  priv->control_channel = String_sprintf("|%s", value);
  priv->control_sp = Sink_new(s(priv->control_channel));
  
  return 0;
}


static int set_cleanup(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;

  if (priv->cleanup) {
    String_free(&priv->cleanup);
  }

  fprintf(stderr, "%s(%s)\n", __func__, value);
  priv->cleanup = String_sprintf("|%s", value);

  return 0;
}


static int handle_token(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;

  String *x = String_sprintf("%s\n", value);
  fprintf(stderr, "%s: %s", __func__, s(x));
  if (priv->control_channel) {
    Sink_write(priv->control_sp, s(x), String_len(x));
    Sink_flush(priv->control_sp);
  }
  else if (priv->proc_sp) {
    Sink_write(priv->proc_sp, s(x), String_len(x));
    Sink_flush(priv->proc_sp);
  }
  String_free(&x);
  
  return 0;
}


static int handle_start(Instance *pi, const char *value_ignored)
{
  SubProc_private *priv = (SubProc_private *)pi;
  
  if (priv->proc_sp) {
    /* FIXME: Close/stop process... */
    Sink_free(&priv->proc_sp);
  }

  priv->proc_sp = Sink_new(s(priv->proc));
  
  return 0;
}


static int handle_stop(Instance *pi, const char *value_ignored)
{
  SubProc_private *priv = (SubProc_private *)pi;

  if (priv->cleanup) {
    system(s(priv->cleanup));
  }

  return 0;
}


static Config config_table[] = {
  { "proc", set_proc, NULL, NULL},
  { "control_channel", set_control_channel, NULL, NULL},
  { "cleanup", set_cleanup, NULL, NULL},

  { "token", handle_token, NULL, NULL},

  { "start", handle_start, NULL, NULL},
  { "stop", handle_stop, NULL, NULL},

};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void RawData_handler(Instance *pi, void *data)
{
  SubProc_private *priv = (SubProc_private *)pi;
  RawData_buffer *raw = data;

  /* Pass raw data to main process sink. */
  if (priv->proc_sp) {
    Sink_write(priv->proc_sp, raw->data, raw->data_length);
  }

  RawData_buffer_discard(raw);
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
  SubProc_private *priv = (SubProc_private *)pi;
  //priv->proc = String_value_none();
  //priv->control_channel = String_value_none();
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
