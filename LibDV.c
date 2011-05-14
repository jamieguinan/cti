/*
 * DV data input.  For reference,
 *   http://dvswitch.alioth.debian.org/wiki/DV_format/
 *   av/soup/nuvx/dvtorgb.c
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */

#include "Template.h"
#include "LibDV.h"
#include "SourceSink.h"
#include "Cfg.h"

#include "libdv/dv.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input LibDV_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_422P };
static Output LibDV_outputs[] = {
  /* FIXME: NTSC output might actually be 4:1:1.  Handle in a good way. */
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  dv_decoder_t *decoder;
  char *input;
  Source *source;
  ArrayU8 *chunk;
  int needData;
  int enable;			/* Set this to start processing. */
  int use_feedback;
  int pending_feedback;
  int feedback_threshold;
  int retry;
} LibDV_private;

static int set_input(Instance *pi, const char *value)
{
  LibDV_private *priv = pi->data;
  if (priv->input) {
    free(priv->input);
  }
  if (priv->source) {
    Source_free(&priv->source);
  }

  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }
  
  priv->input = strdup(value);
  priv->source = Source_new(priv->input);

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  LibDV_private *priv = pi->data;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "LibDV: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("LibDV enable set to %d\n", priv->enable);

  return 0;
}

static int set_use_feedback(Instance *pi, const char *value)
{
  LibDV_private *priv = pi->data;
  priv->use_feedback = atoi(value);
  printf("LibDV use_feedback set to %d\n", priv->use_feedback);
  return 0;
}

static int do_seek(Instance *pi, const char *value)
{
  LibDV_private *priv = pi->data;
  long amount = atol(value);

  if (priv->source) {
    Source_seek(priv->source, amount);  
  }

  /* Reset current stuff. */
  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();
  
  return 0;
}



static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "use_feedback", set_use_feedback, 0L, 0L },
  { "seek", do_seek, 0L, 0L},
};


static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
}

static void LibDV_tick(Instance *pi)
{
  Handler_message *hm;
  LibDV_private *priv = pi->data;
  int sleep_and_return = 0;

  /* FIXME: Could use a wait_flag and set if pending_feedback, but
     might want something like GetData_timeout(), or GetData(pi, >=2)
     for millisecond timeout. */

  hm = GetData(pi, 0);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  if (!priv->enable) {
    sleep_and_return = 1;
  }
  else { 
    if (!priv->source) {
      fprintf(stderr, "LibDV enabled, but no source set!\n");
      priv->enable = 0;
      sleep_and_return = 1;
    }
  }

  if (priv->pending_feedback > priv->feedback_threshold) {
    sleep_and_return = 1;
  }

  if (sleep_and_return) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/100}, NULL);
    return;
  }

  if (priv->needData) {
    ArrayU8 *newChunk;
    /* Network reads should return short numbers if not much data is
       available, so using a size relatively large comparted to an
       audio buffer or Jpeg frame should not cause real-time playback
       to suffer here. */
    newChunk = Source_read(priv->source, 32768);
    if (!newChunk) {
      /* FIXME: EOF on a local file should be restartable.  Maybe
	 socket sources should be restartable, too. */
      Source_close_current(priv->source);
      fprintf(stderr, "%s: source finished.\n", __func__);
      if (priv->retry) {
	fprintf(stderr, "%s: retrying.\n", __func__);
	sleep(1);
	Source_free(&priv->source);
	priv->source = Source_new(priv->input);
      }
      else {
	priv->enable = 0;
      }
      return;
    }

    ArrayU8_append(priv->chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    if (cfg.verbosity) { fprintf(stderr, "needData = 0\n"); }
    priv->needData = 0;
  }


 out:
  /* FIXME: trim consumed data from chunk, reset "current" variables. */


  pi->counter++;
}

static void LibDV_instance_init(Instance *pi)
{
  LibDV_private *priv = Mem_calloc(1, sizeof(*priv));
  priv->decoder = dv_decoder_new(0, 1, 1);
  printf("LibDV decoder @ %p\n", priv->decoder);
  pi->data = priv;
}


static Template LibDV_template = {
  .label = "LibDV",
  .inputs = LibDV_inputs,
  .num_inputs = table_size(LibDV_inputs),
  .outputs = LibDV_outputs,
  .num_outputs = table_size(LibDV_outputs),
  .tick = LibDV_tick,
  .instance_init = LibDV_instance_init,
};

void LibDV_init(void)
{
  Template_register(&LibDV_template);
}
