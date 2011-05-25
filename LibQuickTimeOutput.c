/* Search and replace "Example" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"
#include "Wav.h"
#include "LibQuickTimeOutput.h"
#include <lqt.h>

static void Config_handler(Instance *pi, void *msg);
static void Y422P_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_422P, INPUT_BGR3, INPUT_WAV };
static Input LibQuickTimeOutput_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

//enum { /* OUTPUT_... */ };
static Output LibQuickTimeOutput_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  String *output_name;
  quicktime_t *qt;
  int ycc_track;
  lqt_codec_info_t *ycc_encoder;
  int rgb_track;
  lqt_codec_info_t *rgb_encoder;
  int audio_track;
  lqt_codec_info_t *audio_encoder;
  int avi;
  int avi_set;
} LibQuickTimeOutput_private;


static int set_output(Instance *pi, const char *value)
{
  LibQuickTimeOutput_private *priv = pi->data;

  if (priv->output_name) {
    String_free(&priv->output_name);
  }

  if (priv->qt) {
    quicktime_close(priv->qt);
  }

  priv->output_name = String_new(value);

  priv->qt = quicktime_open(priv->output_name->bytes, 0, 1);

  if (!priv->qt) {
    perror("quicktime_open");
    return 1;
  }

  return 0;
}


static int close_output(Instance *pi, const char *value)
{
  LibQuickTimeOutput_private *priv = pi->data;

  if (priv->qt) {
    quicktime_dump(priv->qt);
    quicktime_close(priv->qt);
  }
  else {
    fprintf(stderr, "LibQuickTimeOutput: no active output\n");
  }

  return 0;
}


static int set_avi(Instance *pi, const char *value)
{
  LibQuickTimeOutput_private *priv = pi->data;

  priv->avi = atoi(value);

  return 0;
}


static Config config_table[] = {
  { "output", set_output, 0L, 0L },
  { "close", close_output, 0L, 0L },
  { "avi", set_avi, 0L, 0L },
};


static void push_ycc(LibQuickTimeOutput_private *priv, Y422P_buffer *ycc)
{
  int rc;
  
  if (!priv->qt) {
    fprintf(stderr, "%s: no output set!\n", __func__);
    return;
  }

  if (!priv->ycc_encoder) {
    fprintf(stderr, "%s: no encoder set!\n", __func__);
    return;
  }

  if (priv->ycc_track == -1) {
    priv->ycc_track = lqt_add_video_track(priv->qt,
			     ycc->width, /* frame_w */
			     ycc->height, /* frame_h */
			     1001,	/* frame_duration */
			     30000,	/* timescale */
			     priv->ycc_encoder);
    
    printf("lqt_add_video_track returns %d\n", priv->ycc_track);
  }

  if (priv->audio_track == -1) {
    return;
  }

  if (priv->avi && !priv->avi_set) {
    quicktime_set_avi(priv->qt, 1);
    priv->avi_set = 1;
  }

  int bytes = ycc->y_length + ycc->cr_length + ycc->cb_length;
  uint8_t *video_buffer = malloc(bytes);
  memcpy(video_buffer, ycc->y, ycc->y_length);
  memset(video_buffer+ycc->y_length, 0, ycc->cr_length);
  memset(video_buffer+ycc->y_length+ycc->cr_length, 0, ycc->cb_length);
  //memcpy(video_buffer+ycc->y_length, ycc->cr, ycc->cr_length);
  //memcpy(video_buffer+ycc->y_length+ycc->cr_length, ycc->cr, ycc->cb_length);

  rc = quicktime_write_frame(priv->qt,
			     video_buffer,
			     bytes,
			     priv->ycc_track
			     );

  free(video_buffer);
}


static void push_rgb(LibQuickTimeOutput_private *priv, RGB3_buffer *rgb)
{
  int rc;
  
  if (!priv->qt) {
    fprintf(stderr, "%s: no output set!\n", __func__);
    return;
  }

  if (!priv->rgb_encoder) {
    fprintf(stderr, "%s: no encoder set!\n", __func__);
    return;
  }

  if (priv->rgb_track == -1) {
    priv->rgb_track = lqt_add_video_track(priv->qt,
			     rgb->width, /* frame_w */
			     rgb->height, /* frame_h */
			     1001,	/* frame_duration */
			     30000,	/* timescale */
			     priv->rgb_encoder);
    
    printf("lqt_add_video_track returns %d\n", priv->rgb_track);
  }

  if (priv->audio_track == -1) {
    return;
  }

  if (priv->avi && !priv->avi_set) {
    quicktime_set_avi(priv->qt, 1);
    priv->avi_set = 1;
  }
  

  uint8_t *video_buffer = malloc(rgb->data_length);
  memcpy(video_buffer, rgb->data, rgb->data_length);

  rc = quicktime_write_frame(priv->qt,
			     video_buffer,
			     rgb->data_length,
			     priv->rgb_track
			     );

  free(video_buffer);
}


static void Y422P_handler(Instance *pi, void *data)
{
  static int frame = 0;
  fprintf(stdout, "\rY422P frame %d", frame);
  fflush(stdout);
  frame += 1;
}

static int frame_limit = 30;

static void BGR3_handler(Instance *pi, void *data)
{
  LibQuickTimeOutput_private *priv = pi->data;
  BGR3_buffer *bgr = data;

#if 0
  Y422P_buffer *ycc = 0L;
  static int frame = 0;

  
  ycc = BGR3_toY422P(bgr);
  BGR3_buffer_discard(bgr);

  if (frame < frame_limit) {
    fprintf(stdout, "\rY422P frame %d", frame+1);
    fflush(stdout);
    push_ycc(priv, ycc);
  }
  else if (frame == frame_limit) {
    quicktime_close(priv->qt);
    priv->qt = 0L;
  }

  frame += 1;

  Y422P_buffer_discard(ycc);
#endif

#if 1
  RGB3_buffer *rgb = 0L;
  static int frame = 0;
  
  bgr3_to_rgb3(&bgr, &rgb);

  if (frame < frame_limit) {
    fprintf(stdout, "\rRGB frame %d", frame+1);
    fflush(stdout);
    push_rgb(priv, rgb);
  }
  else if (frame == frame_limit) {
    quicktime_close(priv->qt);
    priv->qt = 0L;
  }

  frame += 1;

  RGB3_buffer_discard(rgb);
#endif
}

static void Wav_handler(Instance *pi, void *data)
{
  LibQuickTimeOutput_private *priv = pi->data;
  Wav_buffer *wav = data;

 if (!priv->qt) {
    return;
  }

  if (!priv->audio_encoder) {
    fprintf(stderr, "%s: no audio encoder set!\n", __func__);
    return;
  }

  if (priv->audio_track == -1) {
    priv->audio_track = lqt_add_audio_track(priv->qt,
					    wav->params.channels,
					    wav->params.rate,
					    wav->params.bits_per_sample,
					    priv->audio_encoder);
    
    printf("lqt_add_audio_track (%d %d %d) returns %d\n", 
	   wav->params.channels,
	   wav->params.rate,
	   wav->params.bits_per_sample,
	   priv->audio_track);
  }

  if (priv->ycc_track == -1 && priv->rgb_track == -1) {
    return;
  }

  if (priv->avi && !priv->avi_set) {
    quicktime_set_avi(priv->qt, 1);
    priv->avi_set = 1;
  }


  lqt_encode_audio_raw(priv->qt,
		       wav->data,
		       wav->data_length/4,
		       priv->audio_track);
}

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void LibQuickTimeOutput_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void LibQuickTimeOutput_instance_init(Instance *pi)
{
  LibQuickTimeOutput_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;

  int i;
  lqt_codec_info_t** encoders;

  encoders = lqt_find_video_codec(/* fourcc = */ "yuv2", /* encoders = */ 1);
  if (encoders) {
    for (i=0; encoders[i]; i++) {
      printf("  %s (%s)\n", encoders[i]->name, encoders[i]->long_name);
    }
    // priv->ycc_encoder = encoders[0];
    lqt_destroy_codec_info(encoders);
  }

  encoders = lqt_find_video_codec(/* fourcc = */ "2vuy", /* encoders = */ 1);
  if (encoders) {
    for (i=0; encoders[i]; i++) {
      printf("  %s (%s)\n", encoders[i]->name, encoders[i]->long_name);
    }
    priv->ycc_encoder = encoders[0];
    //lqt_destroy_codec_info(encoders);
    priv->ycc_track = -1;		/* Unset. */
  }

  /* "raw " is RGB triplets. */
  encoders = lqt_find_video_codec(/* fourcc = */ "raw ", /* encoders = */ 1);
  if (encoders) {
    for (i=0; encoders[i]; i++) {
      printf("  %s (%s)\n", encoders[i]->name, encoders[i]->long_name);
    }
    priv->rgb_encoder = encoders[0];
  }
  priv->rgb_track = -1;

  // encoders = lqt_find_audio_codec(/* fourcc = */ "twos", /* encoders = */ 1);
  encoders = lqt_find_audio_codec_by_wav_id( /*wav_id = */ 1, /* encoders = */ 1);
  if (encoders) {
    for (i=0; encoders[i]; i++) {
      printf("  %s (%s)\n", encoders[i]->name, encoders[i]->long_name);
    }
    priv->audio_encoder = encoders[0];
  }
  priv->audio_track = -1;

}


static Template LibQuickTimeOutput_template = {
  .label = "LibQuickTimeOutput",
  .inputs = LibQuickTimeOutput_inputs,
  .num_inputs = table_size(LibQuickTimeOutput_inputs),
  .outputs = LibQuickTimeOutput_outputs,
  .num_outputs = table_size(LibQuickTimeOutput_outputs),
  .tick = LibQuickTimeOutput_tick,
  .instance_init = LibQuickTimeOutput_instance_init,
};

void LibQuickTimeOutput_init(void)
{
  Template_register(&LibQuickTimeOutput_template);
  lqt_registry_init();
}

