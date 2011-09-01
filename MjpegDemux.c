/*
 * A/V demuxer.
 */
#include <string.h>		/* memcpy */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* free */
#include <math.h>		/* modf */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "Images.h"
#include "Wav.h"
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
static Input MjpegDemux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_CONTROL ] = { .type_label = "Control_msg", .handler = Control_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
};

enum { OUTPUT_JPEG, OUTPUT_WAV, OUTPUT_O511 };
static Output MjpegDemux_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
  [ OUTPUT_O511 ] = { .type_label = "O511_buffer", .destination = 0L },
  // [ OUTPUT_STATUS ] = { .type_label = "Status_msg", .destination = 0L },
};

enum { PARSING_HEADER, PARSING_DATA };

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
    String *content_type;
    int content_length;
    double timestamp;
    int width;
    int height;
  } current;

  struct {
    double stream_t0;
    double playback_t0;
  } video;

  int seen_audio;

  float rate_multiplier;

  int use_feedback;
  int pending_feedback;
  int feedback_threshold;

  int use_timestamps;

  int retry;

} MjpegDemux_private;


static int set_input(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;
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


static int set_retry(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;
  priv->retry = atoi(value);
  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "MjpegDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("MjpegDemux enable set to %d\n", priv->enable);

  return 0;
}

static int set_use_feedback(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;
  priv->use_feedback = atoi(value);
  printf("MjpegDemux use_feedback set to %d\n", priv->use_feedback);
  return 0;
}


static int set_use_timestamps(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;
  priv->use_timestamps = atoi(value);
  printf("MjpegDemux use_timestamps set to %d\n", priv->use_timestamps);
  return 0;
}



static int do_seek(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = pi->data;
  long amount = atol(value);

  if (priv->source) {
    Source_seek(priv->source, amount);  
  }

  /* Reset current stuff. */
  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->current.content_length = 0;
  priv->current.timestamp = 0.0;
  priv->current.width = 0;
  priv->current.height = 0;
  if (priv->current.content_type) {
    String_free(&priv->current.content_type);
  }
  priv->state = PARSING_HEADER;

  priv->video.stream_t0 = -1.0;
  
  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "retry", set_retry, 0L, 0L },
  { "use_feedback", set_use_feedback, 0L, 0L },
  { "use_timestamps", set_use_timestamps, 0L, 0L },
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
  MjpegDemux_private *priv = pi->data;
  Feedback_buffer *fb = data;

  priv->pending_feedback -= 1;
  //printf("feedback - %d\n", priv->pending_feedback);
  Feedback_buffer_discard(fb);
}


static void MjpegDemux_tick(Instance *pi)
{
  Handler_message *hm;
  MjpegDemux_private *priv = pi->data;
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
      fprintf(stderr, "MjpegDemux enabled, but no source set!\n");
      priv->enable = 0;
      sleep_and_return = 1;
    }
  }

  /* NOTE: Checking output thresholds isn't useful in cases where there is another object
     behind the output that is getting its input queue filled up... */
  if (pi->outputs[OUTPUT_JPEG].destination &&
      pi->outputs[OUTPUT_JPEG].destination->parent->pending_messages > 5) {
    sleep_and_return = 1;
  }

  if (pi->outputs[OUTPUT_O511].destination &&
      pi->outputs[OUTPUT_O511].destination->parent->pending_messages > 5) {
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
	fprintf(stderr, "Source_free\n");
	Source_free(&priv->source);
	fprintf(stderr, "Source_new\n");
	priv->source = Source_new(priv->input);
      }
      else {
	priv->enable = 0;
      }

      /* Reset chunk. */
      if (priv->chunk) {
	ArrayU8_cleanup(&priv->chunk);
      }
      priv->chunk = ArrayU8_new();

      goto out2;
    }

    ArrayU8_append(priv->chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    if (cfg.verbosity) { fprintf(stderr, "needData = 0\n"); }
    priv->needData = 0;
  }

  /*
   * priv->chunk should begin with a boundary string, followed by several 
   * header lines, for example:
   *
   * --0123456789NEXT
   * Content-Type: audio/x-wav
   * Timestamp:1267318275.259539
   * Content-Length: 8192
   *
   * --0123456789NEXT
   * Content-Type: image/jpeg
   * Timestamp:1267318275.296939
   * Content-Length: 9241
   *
   */

  /* Parse out the header lines... */
  if (priv->state == PARSING_HEADER) {
    int eoh = -1;
    int soh = ArrayU8_search(priv->chunk, 0, ArrayU8_temp_const("Content-Type", 4));
    if (soh >= 0)  {
      /* Search for double newline at end of header block. */
      eoh = ArrayU8_search(priv->chunk, soh, ArrayU8_temp_const("\r\n\r\n", 4));
    }

    if (soh == -1 || eoh == -1) {
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
    
    int n, eol;
    for (n=soh; n < eoh; n = eol+2) {
      eol = ArrayU8_search(priv->chunk, n, ArrayU8_temp_const("\r\n", 2));
      if (eol != -1) {
	String *line = ArrayU8_extract_string(priv->chunk, n, eol);
	int a, b;
	
	if ((a = String_find(line, 0, "Content-Type:", &b)) != -1) {
	  String_parse_string(line, b, &priv->current.content_type);
	}
	else if ((a = String_find(line, 0, "Timestamp:", &b)) != -1) {
#if 0
	  int n, dot = 0;
	  n = String_find(line, 0, ".", &dot);
	  if (n != -1  && strlen(line->bytes+dot) == 5) {
	    /* Fix up early mjx files that misformatted Timestamp. */
	    printf("%s -> ", line->bytes);
	    String_cat1(line, " ");
	    memmove(line->bytes+dot+1, line->bytes+dot, 5);
	    line->bytes[dot] = '0';
	    printf("%s\n", line->bytes);
	  }
#endif
	  String_parse_double(line, b, &priv->current.timestamp);
	  printf("%f [%s]\n", priv->current.timestamp, line->bytes);
	}
	else if ((a = String_find(line, 0, "Width:", &b)) != -1) {
	  String_parse_int(line, b, &priv->current.width);
	}
	else if ((a = String_find(line, 0, "Height:", &b)) != -1) {
	  String_parse_int(line, b, &priv->current.height);
	}
	else if ((a = String_find(line, 0, "Content-Length:", &b)) != -1) {
	  String_parse_int(line, b, &priv->current.content_length);
	}
	else if (line->bytes[0] == '-' && line->bytes[1] == '-') {
	  if (!priv->boundary) {
	    priv->boundary = String_dup(line);
	  }
	  else {
	    /* FIXME: Test boundary versus previous boundary.  Or not,
	       if its not important.  Besides, I'm setting soh to "Content-type",
	       which comes after the "--" boundary. */
	  }
	}
	String_free(&line);
      }
    }

    if (priv->current.content_type &&
	priv->current.content_length >= 0) {
      priv->state = PARSING_DATA;
    }
    else {
      fprintf(stderr, "Gah!  Did not get all the headers needed.  What to do?\n");
      goto out;
    }
  }

  if (priv->state != PARSING_DATA) {
    fprintf(stderr, "%s: not in PARSING_DATA state!\n", __func__);
    priv->enable = 0;
    return;
  }

  if (priv->chunk->len < priv->current.eoh + 4 + priv->current.content_length) {
    /* Need more data... */
    if (cfg.verbosity) { fprintf(stderr, "needData = 1\n"); }
    priv->needData = 1;
    return;
  }

  /* Check for known content-types. */

  if (streq(priv->current.content_type->bytes, "audio/x-wav")) {
    /* Extract Wav buffer, */
    Wav_buffer *w = Wav_buffer_from(priv->chunk->data + priv->current.eoh + 4, priv->current.content_length);
    if (!w) {
      fprintf(stderr, "%s: could not extract wav data!\n", __func__);
      goto out;
    }

    if (!priv->seen_audio) {
      priv->seen_audio = 1;
    }

    if (pi->outputs[OUTPUT_WAV].destination) {
      PostData(w, pi->outputs[OUTPUT_WAV].destination);
      if (priv->use_feedback & (1<<0)) {
      	priv->pending_feedback += 1;
	// printf("feedback +1 = %d\n", priv->pending_feedback);
      }
    }
    else {
      fprintf(stderr, "discarding wav data\n");
      Wav_buffer_discard(&w);
    }
  }
  
  else if (streq(priv->current.content_type->bytes, "image/jpeg")) {
    Jpeg_buffer *j = Jpeg_buffer_from(priv->chunk->data + priv->current.eoh + 4, 
				      priv->current.content_length);
    /* Reconstruct timeval timestamp for Jpeg buffer.  Not actually used here, though. */
    j->tv.tv_sec = priv->current.timestamp;
    j->tv.tv_usec = fmod(priv->current.timestamp, 1.0) * 1000000;

    if (pi->outputs[OUTPUT_JPEG].destination) {
      /* Use timestamps if configured to do so, and only if haven't
	 seen any audio, which is normally used with feedback. */
      // printf("%d %d\n", priv->use_timestamps, !priv->seen_audio);
      if (priv->use_timestamps && !priv->seen_audio) {
	struct timeval tv_now;
	double tnow;
	gettimeofday(&tv_now, 0L);
	tnow = timeval_to_double(tv_now);
	
	if (priv->video.stream_t0 < 0.0) {
	  /* New stream, or seek occurred. */
	  priv->video.stream_t0 = priv->current.timestamp;
	  priv->video.playback_t0 = tnow;
	}
	else {
	  double stream_diff = priv->current.timestamp - priv->video.stream_t0;
	  double playback_diff = tnow - priv->video.playback_t0;
	  double delay = stream_diff - playback_diff;
	  if (delay > 0.0) {
	    printf("%f %f\n", stream_diff, playback_diff);
	    nanosleep( double_to_timespec(delay), NULL);
	  }
	}

      }

      PostData(j, pi->outputs[OUTPUT_JPEG].destination);
      if (priv->use_feedback & (1<<1)) {
	priv->pending_feedback += 1;
      }
    }
    else {
      Jpeg_buffer_discard(j);
    }

  }

  else if (streq(priv->current.content_type->bytes, "image/o511")) {
    if (priv->current.width == 0 || priv->current.height == 0) {
      fprintf(stderr, "ov511 header missing width or height (%d, %d)\n",
	      priv->current.width, priv->current.height);
    }
    else if (pi->outputs[OUTPUT_O511].destination) {
      O511_buffer *ob = 0L;
      ob = O511_buffer_from(priv->chunk->data + priv->current.eoh + 4,
					 priv->current.content_length,
					 priv->current.width,
					 priv->current.height);
      PostData(ob, pi->outputs[OUTPUT_O511].destination);
      if (priv->use_feedback & (1<<2)) {
	priv->pending_feedback += 1;
      }
    }
    /* If neither of the above cases were hit, the ov511 block will be
       ignored.*/
  }

  else {
    /* Either unknown content-type, or outputs not connected.  Data will be ignored. */
    fprintf(stderr, "unknown content type\n");
  }

 out:
  /* trim consumed data from chunk, reset "current" variables. */
  ArrayU8_trim_left(priv->chunk, priv->current.eoh + 4 + priv->current.content_length);

 out2:
  priv->current.content_length = 0;
  priv->current.timestamp = 0.0;
  priv->current.width = 0;
  priv->current.height = 0;
  if (priv->current.content_type) {
    String_free(&priv->current.content_type);
  }
  priv->state = PARSING_HEADER;
  pi->counter += 1;
}


static void MjpegDemux_instance_init(Instance *pi)
{
  MjpegDemux_private *priv = Mem_calloc(1, sizeof(*priv));
  priv->rate_multiplier = 1.0;
  priv->max_chunk_size = 1024*1024*4;
  priv->retry = 0;
  priv->use_feedback = 0;
  priv->video.stream_t0 = -1.0;
  priv->feedback_threshold = 20;
  pi->data = priv;
}

static Template MjpegDemux_template = {
  .label = "MjpegDemux",
  .inputs = MjpegDemux_inputs,
  .num_inputs = table_size(MjpegDemux_inputs),
  .outputs = MjpegDemux_outputs,
  .num_outputs = table_size(MjpegDemux_outputs),
  .tick = MjpegDemux_tick,
  .instance_init = MjpegDemux_instance_init,
};

void MjpegDemux_init(void)
{
  Template_register(&MjpegDemux_template);
}
