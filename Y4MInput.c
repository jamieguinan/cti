/*
 * YUV4MPEG[2] input.
 */
#include <string.h>		/* memcpy */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* free */
#include <math.h>		/* modf */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "Images.h"
#include "Mem.h"
#include "String.h"
#include "Array.h"
#include "SourceSink.h"
#include "Cfg.h"
#include "Numbers.h"
#include "Control.h"

static void Config_handler(Instance *pi, void *data);
static void Control_handler(Instance *pi, void *data);
static void Feedback_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_CONTROL, INPUT_FEEDBACK };
static Input Y4MInput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_CONTROL ] = { .type_label = "Control_msg", .handler = Control_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
};

enum { OUTPUT_422P };
static Output Y4MInput_outputs[] = {
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
};

enum { PARSING_HEADER, PARSING_FRAME };

typedef struct {
  char *input;
  Source *source;
  ArrayU8 *chunk;
  String *boundary;
  int state;
  int needData;
  int max_chunk_size;

  int enable;			/* Set this to start processing. */

  struct {
    int eoh;
    double timestamp;
    int width;
    int height;
  } current;

  struct {
    double last_timestamp;
    double nomimal_period;
  } video;

  float rate_multiplier;

  int use_feedback;
  int pending_feedback;
  int feedback_threshold;

  int retry;

} Y4MInput_private;


static int set_input(Instance *pi, const char *value)
{
  Y4MInput_private *priv = pi->data;
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
  Y4MInput_private *priv = pi->data;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "Y4MInput: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("Y4MInput enable set to %d\n", priv->enable);

  return 0;
}

static int set_use_feedback(Instance *pi, const char *value)
{
  Y4MInput_private *priv = pi->data;
  priv->use_feedback = atoi(value);
  printf("Y4MInput use_feedback set to %d\n", priv->use_feedback);
  return 0;
}

static int do_seek(Instance *pi, const char *value)
{
  Y4MInput_private *priv = pi->data;
  long amount = atol(value);

  if (priv->source) {
    Source_seek(priv->source, amount);  
  }

  /* Reset current stuff. */
  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->current.timestamp = 0.0;
  priv->current.width = 0;
  priv->current.height = 0;
  priv->state = PARSING_HEADER;
  
  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "use_feedback", set_use_feedback, 0L, 0L },
  /* The following are more "controls" than "configs", but maybe they are essentially the same anyway. */
  //{ "rate", set_rate, 0L, 0L},
  { "seek", do_seek, 0L, 0L},
  //{ "position", set_position, 0L, 0L},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Control_handler(Instance *pi, void *data)
{
  Control_msg * msg_in;
  msg_in = PopMessage(&pi->inputs[INPUT_CONTROL]);
      
  /* FIXME: Handle message... */
  /* seek_bytes: {beginning, current, end}, amount */

  if (streq(msg_in->label->bytes, "seek+")) {
    /* seek(Value_get_float(msg_in->value)); */
  }
  else if (streq(msg_in->label->bytes, "seek-")) {
    /* seek(Value_get_float(msg_in->value)); */
  }
  else if (streq(msg_in->label->bytes, "seekfactor+")) {
    /* Value_get_int(msg_in->value); */
  }
  else if (streq(msg_in->label->bytes, "seekfactor-")) {
    /* Value_get_int(msg_in->value); */
  }
  Control_msg_discard(msg_in);
}

static void Feedback_handler(Instance *pi, void *data)
{
  Y4MInput_private *priv = pi->data;
  Feedback_buffer *fb = data;

  priv->pending_feedback -= 1;
  //printf("feedback - %d\n", priv->pending_feedback);
  Feedback_buffer_discard(fb);
}


static void Y4MInput_tick(Instance *pi)
{
  Y4MInput_private *priv = pi->data;
  Handler_message *hm;
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
      fprintf(stderr, "Y4MInput enabled, but no source set!\n");
      priv->enable = 0;
      sleep_and_return = 1;
    }
  }
  
  /* NOTE: Checking output thresholds isn't useful in cases where there is another object
     behind the output that is getting its input queue filled up... */
  if (pi->outputs[OUTPUT_422P].destination &&
      pi->outputs[OUTPUT_422P].destination->parent->pending_messages > 5) {
    sleep_and_return = 1;
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
    /* Network reads should return short buffers if not much data is
       available. */
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

  if (priv->state == PARSING_HEADER) {
    int eoh = -1;
    int soh = ArrayU8_search(priv->chunk, 0, ArrayU8_temp_const("YUV4MPEG2", 9));
    if (soh == 0)  {
      /* Search for newline at end of header line. */
      eoh = ArrayU8_search(priv->chunk, soh, ArrayU8_temp_const("\n", 1));
    }

    if (soh != 0 || eoh == -1) {
      if (priv->chunk->len > priv->max_chunk_size) {
	fprintf(stderr, "%s: header size too big, bogus data??\n", __func__);
	priv->enable = 0;
      }
      else {
	/* Need more data, will get it on next call. */
	if (cfg.verbosity) { fprintf(stderr, "needData = 1\n"); }
	priv->needData = 1;
      }
      return;
    }

    priv->current.eoh = eoh;

    /* Parse header line. */
    String *s = ArrayU8_extract_string(priv->chunk, soh, eoh);
    List *ls = String_split(s, " ");
    int i;
    for (i=0; i < ls->things_count; i++) {
      String *t = ls->things[i];
      int width;
      //int width_set = 0;
      int height;
      //int height_set = 0;
      int frame_numerator;
      //int frame_numerator_set = 0;
      int frame_denominator;
      //int frame_denominator_set = 0;
      int aspect_numerator;
      //int aspect_numerator_set = 0;
      int aspect_denominator;
      //int aspect_denominator_set = 0;
      char chroma_subsamping[32+1];
      //int chroma_subsamping_set = 0;
      char interlacing;
      //int interlacing_set = 0;

      if (sscanf(t->bytes, "W%d", &width) == 1) {
      }
      else if (sscanf(t->bytes, "H%d", &height) == 1) {
      }
      else if (sscanf(t->bytes, "C%32s", chroma_subsamping) == 1) {
      }
      else if (sscanf(t->bytes, "I%c", &interlacing) == 1) {
      }
      else if (sscanf(t->bytes, "F%d:%d", &frame_numerator, &frame_denominator) == 2) {
      }
      else if (sscanf(t->bytes, "A%d:%d", &aspect_numerator, &aspect_denominator) == 2) {
      }
      // else if (sscanf(t->bytes, "X%s", metadata) == 1) { }

      /* Free the strings as we go along... */
      String_free(&t);
    }

    List_free(ls);
    
    priv->state = PARSING_FRAME;
  }

  if (priv->state != PARSING_FRAME) {
    fprintf(stderr, "%s: not in PARSING_FRAME state!\n", __func__);
    priv->enable = 0;
    return;
  }

  /* TODO: Parse FRAME line... */

  if (0) {
    /* Need more data... */
    if (cfg.verbosity) { fprintf(stderr, "needData = 1\n"); }
    priv->needData = 1;
    return;
  }

  if (pi->outputs[OUTPUT_422P].destination) {
    //Y422P_buffer *y422p = Y422P_buffer_from(priv->chunk->...);
    //PostData(y422p, pi->outputs[OUTPUT_422P].destination);
    if (priv->use_feedback & (1<<2)) {
      priv->pending_feedback += 1;
    }
  }

  /* trim consumed data from chunk, reset "current" variables. */
  // ArrayU8_trim_left(priv->chunk, priv->current.eoh + 4 + priv->current.content_length);
  // priv->current.content_length = 0;
  priv->current.timestamp = 0.0;
  priv->current.width = 0;
  priv->current.height = 0;
  priv->state = PARSING_HEADER;
  pi->counter += 1;
}


static void Y4MInput_instance_init(Instance *pi)
{
  Y4MInput_private *priv = Mem_calloc(1, sizeof(*priv));
  priv->rate_multiplier = 1.0;
  priv->max_chunk_size = 1024*1024*8; /* 8MB */
  priv->retry = 0;
  priv->use_feedback = 0;
  priv->feedback_threshold = 20;
  pi->data = priv;
}

static Template Y4MInput_template = {
  .label = "Y4MInput",
  .inputs = Y4MInput_inputs,
  .num_inputs = table_size(Y4MInput_inputs),
  .outputs = Y4MInput_outputs,
  .num_outputs = table_size(Y4MInput_outputs),
  .tick = Y4MInput_tick,
  .instance_init = Y4MInput_instance_init,
};

void Y4MInput_init(void)
{
  Template_register(&Y4MInput_template);
}
