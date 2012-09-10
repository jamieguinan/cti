/*
 * DV data input.  For reference,
 *   http://dvswitch.alioth.debian.org/wiki/DV_format/
 *   av/soup/nuvx/dvtorgb.c
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "LibDV.h"
#include "SourceSink.h"
#include "Cfg.h"
#include "Images.h"
#include "Wav.h"
#include "Keycodes.h"

#include "libdv/dv.h"

/* From libdv-1.0.0/libdv/parse.c, this is the larger of the 2 frame sizes (PAL). */
#define MAX_DV_FRAME_SIZE (12 * 150 * 80)

static void Config_handler(Instance *pi, void *msg);
static void Feedback_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_FEEDBACK, INPUT_KEYCODE };
static Input LibDV_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_422P, OUTPUT_RGB3, OUTPUT_WAV };
static Output LibDV_outputs[] = {
  /* FIXME: NTSC output might actually be 4:1:1.  Handle in a good way. */
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
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
  int frame_counter;
  int16_t *audio_buffers[4];
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
  priv->frame_counter = 0;

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

  /* Seek to lower frame boundary. */
  if (priv->decoder->frame_size) {
    amount = (priv->decoder->frame_size * (amount/priv->decoder->frame_size));
  }

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
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Keycode_handler(Instance *pi, void *msg)
{
  Keycode_message *km = msg;
  Keycode_message_cleanup(&km);
}


static void Feedback_handler(Instance *pi, void *data)
{
  LibDV_private *priv = pi->data;
  Feedback_buffer *fb = data;

  priv->pending_feedback -= 1;
  printf("feedback - %d\n", priv->pending_feedback);
  Feedback_buffer_discard(fb);
}



static void consume_data(Instance *pi /* would have preferred to use: LibDV_private *priv */)
{
  int rc;
  LibDV_private *priv = pi->data;

  rc = dv_parse_header(priv->decoder, priv->chunk->data);
  if (rc < 0) { 
    fprintf(stderr, "error parsing header");
    return;
  }

  dv_parse_packs(priv->decoder, priv->chunk->data);

  if (dv_format_wide(priv->decoder)) {
    /* 16:9 */
  }
  else if (dv_format_normal(priv->decoder)) {
    /* 4:3 */
  }
  else {
    /* Unknonwn? */
  }

  rc = dv_frame_changed(priv->decoder);
  // printf("dv_frame_changed returns %d\n", rc);

  // fprintf(stderr, "frame size: %zd\n", priv->decoder->frame_size);

  /* FIXME: Call dv_get_timestamp_int(), etc., use the information returned. */

  if (pi->outputs[OUTPUT_WAV].destination) {
    int i, j;
    int n = 0;
    int ns = dv_get_num_samples(priv->decoder);
    int nc = dv_get_num_channels(priv->decoder);
    int rs = dv_get_raw_samples(priv->decoder, nc);
    int c4 = dv_is_4ch(priv->decoder);
    int fr = dv_get_frequency(priv->decoder);

    rc = dv_decode_full_audio(priv->decoder, priv->chunk->data, priv->audio_buffers);
    if (rc == 0) {
      printf("no audio!\n");
      goto out;
    }

    Wav_buffer *wav = Wav_buffer_new(fr, nc, 2);
    wav->data_length = ns*nc*2;
    wav->data = Mem_malloc(wav->data_length);
    int16_t *wav16 = (int16_t *)wav->data;

    /* Interleave data into wav buffer. */
    for (i=0; i < ns; i++) {
      for (j=0; j < nc; j++) {
	wav16[n++] = priv->audio_buffers[j][i];
      }
    }

    Wav_buffer_finalize(wav);

    if (priv->frame_counter % 30 == 0) {
      printf("samples:%d channels:%d raw samples:%d 4ch:%d fr%d\n",
	     ns, nc, rs, c4, fr);
    }

    PostData(wav, pi->outputs[OUTPUT_WAV].destination);

    if (priv->use_feedback) {
      priv->pending_feedback += 1;
    }

  out:;
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    RGB3_buffer *rgb3 = RGB3_buffer_new(priv->decoder->width, priv->decoder->height);
    uint8_t *pixels[3] = { rgb3->data, 
			   0L,
			   0L };
    int pitches[3] = { rgb3->width * 3, 0, 0 };

    dv_report_video_error (priv->decoder,
                           rgb3->data);
    dv_decode_full_frame(priv->decoder, priv->chunk->data, e_dv_color_rgb, pixels, pitches);
    priv->decoder->prev_frame_decoded = 1;
    PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
  }

  if (pi->outputs[OUTPUT_422P].destination) {
    //uint8_t *pixels[3];
    //int pitches[3] = {???, ???, ???};
    // dv_decode_full_frame(priv->decoder, priv->chunk->data, e_dv_color_yuv, pixels, pitches);
    //PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
  }

  // FIXME: libdv supports 4-byte {B,G,R,0} format, consider supporting in CTI.
  //if (pi->outputs[OUTPUT_BGR0].destination) {
    //uint8_t *pixels[3];
    //pixels[0] = calloc(720*480*3, 1);
    //int pitches[3] = {720*3, 0, 0};
  //  dv_decode_full_frame(priv->decoder, priv->chunk->data, e_dv_color_bgr0, pixels, pitches);
  //}
 

  /* Trim consumed data. */
  ArrayU8_trim_left(priv->chunk, priv->decoder->frame_size);
  priv->frame_counter += 1;
}

static void LibDV_tick(Instance *pi)
{
  Handler_message *hm;
  LibDV_private *priv = pi->data;
  int sleep_and_return = 0;

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

  /* NOTE: Checking output thresholds isn't useful in cases where there is another object
     behind the output that is getting its input queue filled up... */
  if (pi->outputs[OUTPUT_RGB3].destination &&
      pi->outputs[OUTPUT_RGB3].destination->parent->pending_messages > 5) {
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
      fprintf(stderr, "%s: source finished, %d frames processed, frame size %zd.\n",
	      __func__, priv->frame_counter, priv->decoder->frame_size);
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

  if (priv->chunk->len >= MAX_DV_FRAME_SIZE) {
    consume_data(pi);
  }
  else {
    priv->needData = 1;
  }

  pi->counter++;
}

static int LibDV_initialized = 0;
static void LibDV_instance_init(Instance *pi)
{
  LibDV_private *priv = Mem_calloc(1, sizeof(*priv));
  int i;

  /* Could put a lock around this test, in case > 1 instance. */
  if (!LibDV_initialized) {
    dv_init(0, 0);		
    LibDV_initialized = 1;

  }

  priv->decoder = dv_decoder_new(0, 1, 1);
  printf("LibDV decoder @ %p\n", priv->decoder);

  priv->decoder->quality = DV_QUALITY_BEST;
  priv->decoder->prev_frame_decoded = 0;
  priv->feedback_threshold = 2;

  for (i=0; i < 4; i++) {
    priv->audio_buffers[i] = Mem_calloc(1, DV_AUDIO_MAX_SAMPLES*sizeof(int16_t));
  }
  

  dv_set_error_log(priv->decoder, fopen("/dev/null", "w"));
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
