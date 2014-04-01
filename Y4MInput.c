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

static void Config_handler(Instance *pi, void *data);
static void Feedback_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_FEEDBACK };
static Input Y4MInput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
};

enum { OUTPUT_YUV420P };
static Output Y4MInput_outputs[] = {
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
};

enum { PARSING_HEADER, PARSING_FRAME };

typedef struct {
  Instance i;
  String input;
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
  Y4MInput_private *priv = (Y4MInput_private *)pi;
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

  String_set(&priv->input, value);
  priv->source = Source_new(sl(priv->input));

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  Y4MInput_private *priv = (Y4MInput_private *)pi;

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
  Y4MInput_private *priv = (Y4MInput_private *)pi;
  priv->use_feedback = atoi(value);
  printf("Y4MInput use_feedback set to %d\n", priv->use_feedback);
  return 0;
}

static int do_seek(Instance *pi, const char *value)
{
  Y4MInput_private *priv = (Y4MInput_private *)pi;
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


static void Feedback_handler(Instance *pi, void *data)
{
  Y4MInput_private *priv = (Y4MInput_private *)pi;
  Feedback_buffer *fb = data;

  priv->pending_feedback -= 1;
  //printf("feedback - %d\n", priv->pending_feedback);
  Feedback_buffer_discard(fb);
}


static void Y4MInput_tick(Instance *pi)
{
  Y4MInput_private *priv = (Y4MInput_private *)pi;
  Handler_message *hm;
  int wait_flag;

  if (!priv->enable || !priv->source
      || (priv->pending_feedback >= priv->feedback_threshold) ) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable || !priv->source) {
    return;
  }

  if (priv->needData) {
    Source_acquire_data(priv->source, priv->chunk, &priv->needData);
    //printf("read %d bytes\n", priv->chunk->len);
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
    String_list *ls = String_split(s, " ");
    int i;
    for (i=0; i < String_list_len(ls); i++) {
      String *t = String_list_get(ls, i);
      if (String_is_none(t)) {
	continue;
      }

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
	printf("width %d\n", width);
	priv->current.width = width;
      }
      else if (sscanf(t->bytes, "H%d", &height) == 1) {
	printf("height %d\n", height);
	priv->current.height = height;
      }
      else if (sscanf(t->bytes, "C%32s", chroma_subsamping) == 1) {
	printf("chroma subsampling %s\n", chroma_subsamping);
      }
      else if (sscanf(t->bytes, "I%c", &interlacing) == 1) {
	printf("interlacing %c\n", interlacing);
      }
      else if (sscanf(t->bytes, "F%d:%d", &frame_numerator, &frame_denominator) == 2) {
	printf("frame %d:%d\n", frame_numerator, frame_denominator);
      }
      else if (sscanf(t->bytes, "A%d:%d", &aspect_numerator, &aspect_denominator) == 2) {
	printf("aspect %d:%d\n", aspect_numerator, aspect_denominator);
      }
      // else if (sscanf(t->bytes, "X%s", metadata) == 1) { }
    }

    String_list_free(&ls);
    
    priv->state = PARSING_FRAME;
    /* Trim out header plus newline. */
    ArrayU8_trim_left(priv->chunk, eoh+1);
  }

  /* Should always be in PARSING_FRAME state here. */
  if (priv->state != PARSING_FRAME) {
    fprintf(stderr, "%s: not in PARSING_FRAME state!\n", __func__);
    priv->enable = 0;
  }

  int eol = -1;
  int sol = ArrayU8_search(priv->chunk, 0, ArrayU8_temp_const("FRAME", 5));
  //printf("sol=%d\n", sol);
  if (sol == 0)  {
    /* Search for newline at end of FRAME header line. */
    eol = ArrayU8_search(priv->chunk, sol, ArrayU8_temp_const("\n", 1));
  }

  if (sol != 0 || eol == -1) {
    if (priv->chunk->len > priv->max_chunk_size) {
      fprintf(stderr, "%s: header size too big, bogus data??\n", __func__);
      priv->enable = 0;
    }
    else {
      /* Need more data, will get it on next call. */
      priv->needData = 1;
    }
    return;
  }

  /* Data begins after eol. */
  /* FIXME: frame_size should be calculated from subsampling */
  int frame_size = (priv->current.width * priv->current.height) 
    + ((priv->current.width * priv->current.height)/4)
    + ((priv->current.width * priv->current.height)/4);

  if ((priv->chunk->len - eol) < frame_size) {
    priv->needData = 1;
    return;
  }

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    /* FIXME: Could preserve the chunk instead of making a copy here. */
    YUV420P_buffer * yuv = YUV420P_buffer_from(priv->chunk->data+eol+1, 
					   priv->current.width, priv->current.height, NULL);
    PostData(yuv, pi->outputs[OUTPUT_YUV420P].destination);
    //if (priv->use_feedback & (1<<2)) {
    //priv->pending_feedback += 1;
    //}
  }

  /* trim consumed data from chunk, reset "current" variables. */
  ArrayU8_trim_left(priv->chunk, eol+1 + frame_size);

  pi->counter += 1;
}


static void Y4MInput_instance_init(Instance *pi)
{
  Y4MInput_private *priv = (Y4MInput_private *)pi;
  priv->rate_multiplier = 1.0;
  priv->max_chunk_size = 1024*1024*8; /* 8MB */
  priv->retry = 0;
  priv->use_feedback = 0;
  priv->feedback_threshold = 20;
  
}

static Template Y4MInput_template = {
  .label = "Y4MInput",
  .priv_size = sizeof(Y4MInput_private),
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
