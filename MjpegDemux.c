/*
 * Demuxer for MJPEG streams, basically jpeg files prefixed by HTTP headers.
 *    http://en.wikipedia.org/wiki/Motion_JPEG
 * Handles a few other content types in addition to image/jpeg.
 * For external examples, search "inurl:axis-cgi/mjpg/video.cgi", although
 * this module requires complete Jpeg files and fails on streams that
 * elide common header data.
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
#include "Keycodes.h"
#include "FPS.h"

static void Config_handler(Instance *pi, void *data);
static void Feedback_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_FEEDBACK, INPUT_KEYCODE };
static Input MjpegDemux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_JPEG, OUTPUT_WAV, OUTPUT_O511, OUTPUT_RAWDATA };
static Output MjpegDemux_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
  [ OUTPUT_O511 ] = { .type_label = "O511_buffer", .destination = 0L },
  [ OUTPUT_RAWDATA ] =  { .type_label = "RawData_buffer", .destination = 0L },
  // [ OUTPUT_STATUS ] = { .type_label = "Status_msg", .destination = 0L },
};

enum { PARSING_HEADER, PARSING_DATA };

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

  String output;			/* Side channel. */
  Sink *output_sink;

  long stop_source_at;			/* Stop and close source once it passes this offset. */

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

  int use_fixed_video_period;
  float fixed_video_period;
  int seen_audio;
  float rate_multiplier;
  int use_feedback;
  int pending_feedback;
  int feedback_threshold;
  int use_timestamps;
  int retry;
  long a, b;			/* For a/b looping. */
  long seek_amount;		/* For forware/back seeks. */
  FPS fps;
  int snapshot;

  int eof_notify;		/* Whether to notify outputs of EOF condition. */
  int eof_notified;		/* Set after notification. */
  struct {
    /* Save a copy of first frames for EOF notification. */
    Jpeg_buffer *jpeg;
    Wav_buffer *wav;
  } buffer;
} MjpegDemux_private;


static int set_input(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;

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


static int set_output(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  if (priv->output_sink) {
    Sink_free(&priv->output_sink);
  }

  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }

  String_set(&priv->output, value);
  priv->output_sink = Sink_new(sl(priv->output));

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "MjpegDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("MjpegDemux enable set to %d\n", priv->enable);

  return 0;
}


static int set_fixed_video_period(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  priv->use_fixed_video_period = 1;
  priv->fixed_video_period = atof(value);
  return 0;
}

static void reset_current(MjpegDemux_private *priv)
{
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
}


static int do_seek(Instance *pi, const char *value)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  long amount = atol(value);

  if (priv->source) {
    Source_seek(priv->source, amount);  
  }

  reset_current(priv);
  
  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "output", set_output, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "retry", 0L, 0L, 0L, cti_set_int, offsetof(MjpegDemux_private, retry) },
  { "fixed_video_period", set_fixed_video_period, 0L, 0L },
  { "use_feedback", 0L, 0L, 0L, cti_set_int, offsetof(MjpegDemux_private, use_feedback) },
  { "use_timestamps", 0L, 0L, 0L, cti_set_int, offsetof(MjpegDemux_private, use_timestamps) },
  /* The following are more "controls" than "configs", but maybe they are essentially the same anyway. */
  { "seek", do_seek, 0L, 0L},
  //{ "position", set_position, 0L, 0L},
  { "eof_notify", 0L, 0L, 0L, cti_set_int, offsetof(MjpegDemux_private, eof_notify) },
  { "stop_source_at", 0L, 0L, 0L, cti_set_long, offsetof(MjpegDemux_private, stop_source_at) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Keycode_handler(Instance *pi, void *msg)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  Keycode_message *km = msg;
  
  if (km->keycode == CTI__KEY_A && priv->source) {
    priv->a = Source_tell(priv->source);
    priv->b = 0;		/* reset */
    printf("a:%ld b:%ld\n", priv->a, priv->b);
  }

  else if (km->keycode == CTI__KEY_B && priv->source && (priv->a != -1) ) {
    priv->b = Source_tell(priv->source);
    if (priv->b < priv->a) {
      priv->a = priv->b = 0;
    }
    printf("a:%ld b:%ld\n", priv->a, priv->b);
  }
  else if (km->keycode == CTI__KEY_UP) {
    priv->seek_amount = cti_min(priv->seek_amount*2, 1000000000); 
    fprintf(stderr, "seek_amount=%ld\n", priv->seek_amount);
  }
  else if (km->keycode == CTI__KEY_DOWN) {
    priv->seek_amount = cti_max(priv->seek_amount/2, 1); 
    fprintf(stderr, "seek_amount=%ld\n", priv->seek_amount);
  }
  else if (km->keycode == CTI__KEY_LEFT) {
    if (priv->source) {
      Source_seek(priv->source, -priv->seek_amount);
      reset_current(priv);
    }
  }
  else if (km->keycode == CTI__KEY_RIGHT) {
    if (priv->source) {
      Source_seek(priv->source, priv->seek_amount);  
      reset_current(priv);
    }
  }

  else if (km->keycode == CTI__KEY_S) {
    if (priv->source) {
      priv->snapshot = 1;
    }
  }

  Keycode_message_cleanup(&km);
}


static void Feedback_handler(Instance *pi, void *data)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  Feedback_buffer *fb = data;

  priv->pending_feedback -= 1;
  //printf("feedback - %d\n", priv->pending_feedback);
  Feedback_buffer_discard(fb);
}

static void notify_outputs_eof(Instance *pi)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  /* Send buffered messages with EOF flag set. */
  if (pi->outputs[OUTPUT_JPEG].destination && priv->buffer.jpeg) {
    priv->buffer.jpeg->c.eof = 1;
    PostData(priv->buffer.jpeg, pi->outputs[OUTPUT_JPEG].destination);
    priv->buffer.jpeg = NULL;
  }
  if (pi->outputs[OUTPUT_WAV].destination && priv->buffer.wav) {
    priv->buffer.wav->eof = 1;
    PostData(priv->buffer.wav, pi->outputs[OUTPUT_WAV].destination);
    priv->buffer.wav = NULL;
  }
  if (pi->outputs[OUTPUT_O511].destination) {
  }
  if (pi->outputs[OUTPUT_RAWDATA].destination) {
  }
}  


static void MjpegDemux_tick(Instance *pi)
{
  Handler_message *hm;
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  int sleep_and_return = 0;

  /* FIXME: Could use a wait_flag and set if pending_feedback, but
     might want something like GetData_timeout(), or GetData(pi, >=2)
     for millisecond timeout. */

  hm = GetData(pi, 0);		/* This has to be 0 for mjxplay... */

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
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

  if (priv->pending_feedback > priv->feedback_threshold) {
    sleep_and_return = 1;
  }

  if (!sleep_and_return && priv->needData) {
    /* Read data, with option to sleep and return on EOF. */
    Source_acquire_data(priv->source, priv->chunk, &priv->needData);

    if (priv->stop_source_at && Source_tell(priv->source) > priv->stop_source_at) {
      Source_close_current(priv->source);
      priv->source->eof = 1;
    }

    if (priv->source->eof) {
      if (priv->eof_notify && !priv->eof_notified) {
	notify_outputs_eof(pi);
	priv->eof_notified = 1;
      }

      if (priv->retry) {
	fprintf(stderr, "%s: retrying\n", __func__);
	Source_close_current(priv->source);
	Source_free(&priv->source);
	priv->source = Source_new(sl(priv->input));
      }
      sleep_and_return = 1;
    }
  }

  if (sleep_and_return) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/30}, NULL);
    return;
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
	// dpf("needData = 1\n");
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
	  String_parse_double(line, b, &priv->current.timestamp);
	  if (priv->use_timestamps && priv->current.timestamp <= 0.001) {
	    /* Some of my early recordings were messed up, so disable
	       timestamp checking. */
	    priv->use_timestamps = 0;
	    // priv->use_feedback = 0;
	  }
	  // printf("%f [%s]\n", priv->current.timestamp, line->bytes);
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
	else if (String_begins_with(line, "--")) {
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
      fprintf(stderr, "  priv->current.content_type=%p\n", priv->current.content_type);
      fprintf(stderr, "  priv->current.content_length=%d\n", priv->current.content_length);
      fprintf(stderr, "  source offset=%ld\n", Source_tell(priv->source));
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
    // dpf("needData = 1\n");
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

    if (priv->eof_notify && !priv->buffer.wav) {
      /* Save a copy of the first frame. */
      priv->buffer.wav = Wav_buffer_from(priv->chunk->data + priv->current.eoh + 4, priv->current.content_length);
    }

    if (!priv->seen_audio) {
      priv->seen_audio = 1;
    }

    if (pi->outputs[OUTPUT_WAV].destination) {
      while (pi->outputs[OUTPUT_WAV].destination->parent->pending_messages > 250) {
	/* Throttle output.  25ms sleep. */
	// printf("MjpegDemux throttle on OUTPUT_WAV\n");
	nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 25 * 1000 * 1000}, NULL);
      }
      PostData(w, pi->outputs[OUTPUT_WAV].destination);
      if (priv->use_feedback & (1<<0)) {
      	priv->pending_feedback += 1;
	// printf("feedback +1 = %d\n", priv->pending_feedback);
      }
    }
    else {
      // fprintf(stderr, "discarding wav data\n");
      Wav_buffer_release(&w);
    }
  }
  
  else if (streq(priv->current.content_type->bytes, "image/jpeg")) {
    Jpeg_buffer *j = Jpeg_buffer_from(priv->chunk->data + priv->current.eoh + 4, 
				      priv->current.content_length, 0L);

    if (priv->eof_notify && !priv->buffer.jpeg) {
      /* Save a copy of the first frame. */
      priv->buffer.jpeg = Jpeg_buffer_from(priv->chunk->data + priv->current.eoh + 4, 
					   priv->current.content_length, 0L);
    }

    /* Reconstruct timeval timestamp for Jpeg buffer.  Not actually used here, though. */
    j->c.tv.tv_sec = priv->current.timestamp;
    j->c.tv.tv_usec = fmod(priv->current.timestamp, 1.0) * 1000000;

    FPS_show(&priv->fps);

    /* FIXME: This is a hack for testing TV streams, even though it should work transparently
       for progressive sources, with some overhead. */
    //j->c.interlace_mode = IMAGE_INTERLACE_TOP_FIRST;
    j->c.interlace_mode = IMAGE_INTERLACE_NONE;

    if (priv->snapshot) {
      FILE *f = fopen("snapshot.jpg", "wb");
      fwrite(j->data, j->encoded_length, 1, f);
      fclose(f);
      priv->snapshot = 0;
      fprintf(stderr, "snapshot saved\n");
    }

    if (pi->outputs[OUTPUT_JPEG].destination) {
      /* Use timestamps if configured to do so, and only if haven't
	 seen any audio, which is normally used with feedback. */
      // printf("%d %d\n", priv->use_timestamps, !priv->seen_audio);

      while (pi->outputs[OUTPUT_JPEG].destination->parent->pending_messages > 250) {
	/* Throttle output.  25ms sleep. */
	nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 25 * 1000 * 1000}, NULL);
	//printf("MjpegDemux throttle on OUTPUT_JPEG\n");
      }

      if (priv->use_fixed_video_period) {
	printf("fixed video period..\n");
	nanosleep( double_to_timespec(priv->fixed_video_period), NULL);	
      }
      else if (priv->use_timestamps && !priv->seen_audio) {
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
	    dpf("%f %f\n", stream_diff, playback_diff);
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
			    priv->current.height,
			    0L);
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
  /* Trim consumed data from chunk, but also copy it to rawdata output if set. */
  if (pi->outputs[OUTPUT_RAWDATA].destination) {
    int size = (priv->current.eoh + 4 + priv->current.content_length);
    RawData_buffer *raw = RawData_buffer_new(size);
    memcpy(raw->data, priv->chunk->data, size);
    PostData(raw, pi->outputs[OUTPUT_RAWDATA].destination);
  }

  if (priv->output_sink) {
    Sink_write(priv->output_sink, priv->chunk->data, priv->current.eoh + 4 + priv->current.content_length);
  }
  
  ArrayU8_trim_left(priv->chunk, priv->current.eoh + 4 + priv->current.content_length);

  /* Reset "current" variables */
  priv->current.content_length = 0;
  priv->current.timestamp = 0.0;
  priv->current.width = 0;
  priv->current.height = 0;
  if (priv->current.content_type) {
    String_free(&priv->current.content_type);
  }
  priv->state = PARSING_HEADER;
  pi->counter += 1;

  /* ab loop */
  if (priv->b && (Source_tell(priv->source) > priv->b)) {
    Source_set_offset(priv->source, priv->a);
  }

}


static void MjpegDemux_instance_init(Instance *pi)
{
  MjpegDemux_private *priv = (MjpegDemux_private *)pi;
  priv->rate_multiplier = 1.0;
  priv->max_chunk_size = 1024*1024*4;
  priv->retry = 0;
  priv->use_feedback = 0;
  priv->video.stream_t0 = -1.0;
  priv->feedback_threshold = 20;
  
  priv->seek_amount = 10000000;
}

static Template MjpegDemux_template = {
  .label = "MjpegDemux",
  .priv_size = sizeof(MjpegDemux_private),
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
