/* 
 * Read from a file or socket, and pass raw data to output. 
 * Restart if EOF on source.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "SourceSink.h"
#include "RawSource.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input RawSource_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_RAWDATA, OUTPUT_CONFIG };
static Output RawSource_outputs[] = {
  [ OUTPUT_RAWDATA ] = { .type_label = "RawData_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  String input;
  Source *source;
  int read_timeout;
} RawSource_private;


static int set_input(Instance *pi, const char *value)
{
  RawSource_private *priv = (RawSource_private *)pi;
  if (priv->source) {
    Source_free(&priv->source);
  }
  String_set(&priv->input, value);
  priv->source = Source_new(s(priv->input));

  if (priv->source && pi->outputs[OUTPUT_CONFIG].destination) {
    /* New source activated, send reset message. */
  }

  return 0;
}


static Config config_table[] = {
  { "input",    set_input, 0L, 0L },
  { "read_timeout",  0L, 0L, 0L, cti_set_int, offsetof(RawSource_private, read_timeout) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void RawSource_move_data(Instance *pi)
{
  RawSource_private *priv = (RawSource_private *)pi;
  ArrayU8 *chunk;
  int needData = 1;

  if (priv->read_timeout && Source_poll_read(priv->source, priv->read_timeout) == 0) {
    /* No read activity, assume stalled. */
    Source_close_current(priv->source);
    set_input(pi, s(priv->input));
  }
  
  if (!priv->source) {
    return;
  }

  chunk = ArrayU8_new();
  Source_acquire_data(priv->source, chunk, &needData);

  if (priv->source->eof) {
    Source_close_current(priv->source);
    set_input(pi, s(priv->input));
  }
  
  if (priv->source && pi->outputs[OUTPUT_RAWDATA].destination) {
    RawData_buffer *raw = RawData_buffer_new(chunk->len);
    memcpy(raw->data, chunk->data, chunk->len);
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }
  ArrayU8_cleanup(&chunk);
}


static void RawSource_tick(Instance *pi)
{
  Handler_message *hm;
  RawSource_private *priv = (RawSource_private *)pi;
  int wait_flag;

  if (!priv->source && s(priv->input)) {
    /* Retry. */
    set_input(pi, s(priv->input));      
  }

if (!s(priv->input)) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  if (priv->source) {
    RawSource_move_data(pi);
  }
  else {
    /* Avoid spinning. */
    sleep(1);
  }

  pi->counter++;
}


static void RawSource_instance_init(Instance *pi)
{
  // RawSource_private *priv = (RawSource_private *)pi;
}


static Template RawSource_template = {
  .label = "RawSource",
  .inputs = RawSource_inputs,
  .num_inputs = table_size(RawSource_inputs),
  .outputs = RawSource_outputs,
  .num_outputs = table_size(RawSource_outputs),
  .tick = RawSource_tick,
  .instance_init = RawSource_instance_init,
};

void RawSource_init(void)
{
  Template_register(&RawSource_template);
}
