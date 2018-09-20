/* Sub-process module. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

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

typedef enum { SUBPROC_NONE, SUBPROC_READ, SUBPROC_WRITE, SUBPROC_FANCY  } SubProc_mode;

typedef struct {
  Instance i;

  /* The sub-process command to run. */
  String *proc;

  SubProc_mode mode;

  /* Optional connections for stdin/stdout/stderr. */
  Sink *proc_stdin;
  Source *proc_stdout;
  Source *proc_stderr;

  /* Fancy sub-process pid/handle. */
  union {
    void *handle;
    int pid;
  } subproc_handle;

  /* Some programs might want separate control input, such as
     mplayer's slave mode.*/
  String *control_channel;
  Sink *control_sp;

  /* Invocation for a cleanup command. */
  String *cleanup;
} SubProc_private;


static void close_all(SubProc_private *priv)
{
  if (priv->proc_stdin) {
    Sink_free(&priv->proc_stdin);
  }

  if (priv->proc_stdout) {
    Source_free(&priv->proc_stdout);
  }

  if (priv->proc_stderr) {
    Source_free(&priv->proc_stderr);
  }

  if (priv->control_sp) {
    Sink_free(&priv->control_sp);
  }

  if (priv->cleanup) {
    system(s(priv->cleanup));
  }

  priv->mode = SUBPROC_NONE;
}


static int set_proc(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;

  if (priv->proc) {
    String_free(&priv->proc);
  }

  fprintf(stderr, "%s(%s)\n", __func__, value);
  /* Prepend pipe character for eventual call to io_open(). */
  priv->proc = String_sprintf("|%s", value);

  return 0;
}


static int set_control_channel(Instance *pi, const char *value)
{
  SubProc_private *priv = (SubProc_private *)pi;

  if (priv->control_channel) {
    // String_unref(&priv->control_channel);
  }

  fprintf(stderr, "%s(%s)\n", __func__, value);
  priv->control_channel = String_sprintf("|%s", value);

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

  String *token = String_sprintf("%s\n", value);
  fprintf(stderr, "%s: %s", __func__, s(token));

  if (priv->control_channel) {
    /* Prefer writing to the control channel. */
    fprintf(stderr, "%s: writing to subproc control channel\n", __func__);
    Sink_write(priv->control_sp, s(token), String_len(token));
    Sink_flush(priv->control_sp);
    fprintf(stderr, "%s: subproc control channel flushed\n", __func__);
  }
  else if (priv->proc_stdin) {
    /* But write to stdin as a fallback. */
    fprintf(stderr, "%s: writing to subproc stdin\n", __func__);
    Sink_write(priv->proc_stdin, s(token), String_len(token));
    Sink_flush(priv->proc_stdin);
    fprintf(stderr, "%s: subproc stdin flushed\n", __func__);
  }
  String_free(&token);

  return 0;
}


static int handle_start_readonly(Instance *pi, const char *optional_param)
{
  SubProc_private *priv = (SubProc_private *)pi;

  close_all(priv);

  if (streq(optional_param, "1")) {
    /* Simple case, enable. */
    priv->proc_stdout = Source_new(s(priv->proc));
  }
  else {
    /* Enable with parameter. */
    String * source_path = String_sprintf("%s %s", s(priv->proc), optional_param);
    priv->proc_stdout = Source_new(s(source_path));
    String_free(&source_path);
  }

  if (!priv->proc_stdout) {
    return 1;
  }

  priv->mode = SUBPROC_READ;

  if (priv->control_channel) {
    priv->control_sp = Sink_new(s(priv->control_channel));
  }

  return 0;
}


static int handle_start_writeonly(Instance *pi, const char *optional_param)
{
  SubProc_private *priv = (SubProc_private *)pi;

  close_all(priv);

  if (streq(optional_param, "1")) {
    /* Simple case, enable. */
    priv->proc_stdin = Sink_new(s(priv->proc));
  }
  else {
    /* Enable with parameter. */
    String * sink_path = String_sprintf("%s %s", s(priv->proc), optional_param);
    priv->proc_stdin = Sink_new(s(sink_path));
    String_free(&sink_path);
  }

  if (!priv->proc_stdin) {
    return 1;
  }

  if (priv->control_channel) {
    priv->control_sp = Sink_new(s(priv->control_channel));
  }

  priv->mode = SUBPROC_WRITE;
  return 0;
}


static int handle_start_fancy(Instance *pi, const char *value_ignored)
{
  SubProc_private *priv = (SubProc_private *)pi;
  close_all(priv);
  fprintf(stderr, "%s is not implemented yet!\n", __func__);
  priv->mode = SUBPROC_NONE;
  return 0;
}


static int handle_stop(Instance *pi, const char *value_ignored)
{
  SubProc_private *priv = (SubProc_private *)pi;

  close_all(priv);

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

  { "start_readonly", handle_start_readonly, NULL, NULL},
  { "start_writeonly", handle_start_writeonly, NULL, NULL},
  { "start_fancy", handle_start_fancy, NULL, NULL},
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
  if (priv->proc_stdin) {
    Sink_write(priv->proc_stdin, raw->data, raw->data_length);
  }

  RawData_buffer_release(raw);
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
