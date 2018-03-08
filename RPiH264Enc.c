/* Interface to Raspberry PI OMX H264 encoder. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "RPiH264Enc.h"
#include "Images.h"
#include "ArrayU8.h"

struct appctx;			/* Type for opaque pointers. */

/* These externs match the functions in "rpi-encode-yuv.c" */
extern void encode_init(struct appctx ** p_ctx, 
			int video_width, int video_height, int video_framerate, int gop_seconds, int video_bitrate);
extern void rpi_get_sizes(struct appctx * ctx, int * y_size, int * u_size, int * v_size);
extern void do_frame_io(struct appctx * ctx,
			uint8_t * y_in, int y_size,
			uint8_t * u_in, int u_size,
			uint8_t * v_in, int v_size,
			uint8_t ** encoded_out, int * encoded_len,
			int * keyframe);
extern void encode_cleanup(struct appctx * ctx);
extern int rpi_encode_yuv_c__analysis_enabled;
extern int say_verbose;
#define NAL_type(p) ((p)[4] & 31)


/* Handlers. */
static void Config_handler(Instance *pi, void *msg);
static void y420p_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV420P, INPUT_YUV422P };
static Input RPiH264Enc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = y420p_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_H264, OUTPUT_FEEDBACK };
static Output RPiH264Enc_outputs[] = {
  [ OUTPUT_H264 ] = { .type_label = "H264_buffer", .destination = 0L },
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L  },
};

typedef struct {
  Instance i;
  struct appctx * ctx;
  int initialized;
  int video_width;
  int video_height;
  int video_framerate;
  int gop_seconds;
  int video_bitrate;
} RPiH264Enc_private;

static Config config_table[] = {
  { "fps", 0L, 0L, 0L, cti_set_int, offsetof(RPiH264Enc_private, video_framerate) },
  { "bitrate", 0L, 0L, 0L, cti_set_int, offsetof(RPiH264Enc_private, video_bitrate) },
  { "gop_seconds", 0L, 0L, 0L, cti_set_int, offsetof(RPiH264Enc_private, gop_seconds) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void y420p_handler(Instance *pi, void *msg)
{
  RPiH264Enc_private *priv = (RPiH264Enc_private *)pi;
  YUV420P_buffer *y420p = msg;
  int keyframe = 0;
  
  if (!priv->initialized) {
    priv->video_width = y420p->width;
    priv->video_height = y420p->height;
    if (y420p->c.fps_denominator) {
      priv->video_framerate = y420p->c.fps_numerator/y420p->c.fps_denominator;
    }
    encode_init(&priv->ctx, 
		priv->video_width,
		priv->video_height,
		priv->video_framerate, 
		priv->gop_seconds,
		priv->video_bitrate);
    priv->initialized = 1;
  }

  uint8_t *output = NULL;
  int output_size = 0;
  
  int y_size, u_size, v_size;
  rpi_get_sizes(priv->ctx, &y_size, &u_size, &v_size);
  do_frame_io(priv->ctx,
	      y420p->y, y_size,
	      y420p->cb, u_size,
	      y420p->cr, v_size,
	      &output, &output_size,
	      &keyframe);
  
  if (output) {
    if (pi->outputs[OUTPUT_H264].destination) {
      H264_buffer *hout = H264_buffer_from(output, output_size, y420p->width, y420p->height, &y420p->c);
      hout->keyframe = keyframe;
      PostData(hout, pi->outputs[OUTPUT_H264].destination);      
    }
    free(output);
  }
  
  /* Try a second call to flush output buffer. */
  output = NULL;
  do_frame_io(priv->ctx,
	      NULL, 0,
	      NULL, 0,
	      NULL, 0,
	      &output, &output_size,
	      &keyframe);

  if (output) {
    dpf("do_frame_io second try succeeded\n");
    if (pi->outputs[OUTPUT_H264].destination) {
      H264_buffer *hout = H264_buffer_from(output, output_size, y420p->width, y420p->height, &y420p->c);
      hout->keyframe = keyframe;
      PostData(hout, pi->outputs[OUTPUT_H264].destination);      
    }
    free(output);
  }
  
  YUV420P_buffer_release(y420p);
}


static void y422p_handler(Instance *pi, void *msg)
{
  YUV422P_buffer *y422p = msg;
  YUV420P_buffer *y420 = YUV422P_to_YUV420P(y422p);
  y420p_handler(pi, y420);
  YUV422P_buffer_release(y422p);  
}


static void RPiH264Enc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void RPiH264Enc_instance_init(Instance *pi)
{
  RPiH264Enc_private *priv = (RPiH264Enc_private *)pi;
  /* Set some sensible defaults. */
  priv->video_framerate = 15;
  priv->video_bitrate = 500000;
  priv->gop_seconds = 5;

  say_verbose = 1;
  // rpi_encode_yuv_c__analysis_enabled = 1;
}


static Template RPiH264Enc_template = {
  .label = "RPiH264Enc",
  .priv_size = sizeof(RPiH264Enc_private),  
  .inputs = RPiH264Enc_inputs,
  .num_inputs = table_size(RPiH264Enc_inputs),
  .outputs = RPiH264Enc_outputs,
  .num_outputs = table_size(RPiH264Enc_outputs),
  .tick = RPiH264Enc_tick,
  .instance_init = RPiH264Enc_instance_init,
};

void RPiH264Enc_init(void)
{
  Template_register(&RPiH264Enc_template);
}
