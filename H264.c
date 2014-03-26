/* H264 encoder using libx264. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#ifndef __tune_pentium4__

#include "CTI.h"
#include "H264.h"
#include "Images.h"
#include "x264.h"
//#include "output/output.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);
static void Y420P_handler(Instance *pi, void *msg);
static void Y422P_handler(Instance *pi, void *msg);

/* x264.h says "nothing other than I420 is really supported", so use X264_CSP_I420 */
enum { INPUT_CONFIG, INPUT_420P, INPUT_422P };
static Input H264_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_420P ] = { .type_label = "420P_buffer", .handler = Y420P_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
};

enum { OUTPUT_H264, OUTPUT_FEEDBACK };
static Output H264_outputs[] = {
  [ OUTPUT_H264 ] = { .type_label = "H264_buffer", .destination = 0L },
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L  },
};

typedef struct {
  Instance i;
  int pts;
  x264_t *encoder;
  String output;			/* Side channel. */
  Sink *output_sink;

  /* Similar to as used in x264.c: */
  //cli_output_t cli_output;
  //hnd_t hout;

} H264_private;


static int set_output(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;
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


static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
  { "output",    set_output, 0L, 0L }
};


static void Y420P_handler(Instance *pi, void *msg)
{
  int rc;
  H264_private *priv = (H264_private *)pi;
  Y420P_buffer *y420 = msg;

  if (!priv->encoder) {
    /* Set up the encoder and parameters. */
    x264_param_t params;

    /* FIXME: Could make preset and profile settings config options
       during setup, and return available values from
       x264_preset_names[], x264_tune_names[], and
       x264_profile_names[] as defined in "x264.h".  */
    rc = x264_param_default_preset(&params, "medium", "psnr");
    if (rc != 0) { fprintf(stderr, "x264_param_default_preset failed");  return; }

    rc = x264_param_apply_profile(&params, "baseline");
    if (rc != 0) { fprintf(stderr, "x264_param_apply_profile failed");  return; }

    params.i_width = y420->width;
    params.i_height = y420->height;

    /* FIXME: Set this in cmd file, or via Config messages. */
    params.i_fps_num = 25;
    params.i_fps_den = 1;

    priv->encoder = x264_encoder_open(&params);
    if (!priv->encoder) { fprintf(stderr, "x264_encoder_open failed");  return; }

    printf("params fps=%d/%d\n", params.i_fps_num, params.i_fps_den);

    //priv->cli_output = mp4_output;
    //cli_output_opt_t output_opt = {};
    //rc = priv->cli_output.open_file("output.mp4", &priv->hout, &output_opt);
    //printf("cli_output.open_file returns %d\n", rc);
  }

  {
    x264_picture_t pic_in = {};
    x264_picture_t pic_out = {};
    x264_nal_t *nal = 0L;
    int pi_nal = 0;

    x264_picture_init(&pic_in);
    pic_in.img.i_csp = X264_CSP_I420;
    pic_in.img.i_plane = 3;
    pic_in.img.i_stride[0] = y420->width;
    pic_in.img.i_stride[1] = y420->width/2;
    pic_in.img.i_stride[2] = y420->width/2;
    pic_in.img.plane[0] = y420->y;
    pic_in.img.plane[1] = y420->cb;
    pic_in.img.plane[2] = y420->cr;

    /* FIXME: Set pts and dts correctly. */
    pic_in.i_pts = priv->pts;
    priv->pts += 1;

    int frame_size = x264_encoder_encode(priv->encoder, &nal, &pi_nal, &pic_in, &pic_out );

    //dpf
    printf
      ("frame_size=%d pi_nal=%d nal=%p p_payload=%p i_payload=%d\n", frame_size, pi_nal, nal,
	nal ? (nal->p_payload) : NULL,
	nal ? (nal->i_payload) : 0
	);

    if (frame_size > 0 && pi->outputs[OUTPUT_H264].destination) {
      H264_buffer *hout = H264_buffer_from(nal[0].p_payload, frame_size, y420->width, y420->height, &y420->c);
      PostData(hout, pi->outputs[OUTPUT_H264].destination);
    }

    if (frame_size > 0 && sl(priv->output)) {
      //Sink_write(priv->output_sink, nal[0].p_payload, frame_size);
    }

    if (frame_size > 0) {
      // i_frame_size = cli_output.write_frame( priv->hout, nal[0].p_payload, i_frame_size, &pic_out );
      // *last_dts = pic_out.i_dts;
    }

  }

  Y420P_buffer_discard(y420);
}

static void Y422P_handler(Instance *pi, void *msg)
{
  // int rc;
  Y422P_buffer *y422 = msg;
  Y420P_buffer *y420 = Y422P_to_Y420P(y422);
  Y420P_handler(pi, y420);
  Y422P_buffer_discard(y422);
}



static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void H264_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void H264_instance_init(Instance *pi)
{

}


static Template H264_template = {
  .label = "H264",
  .priv_size = sizeof(H264_private),
  .inputs = H264_inputs,
  .num_inputs = table_size(H264_inputs),
  .outputs = H264_outputs,
  .num_outputs = table_size(H264_outputs),
  .tick = H264_tick,
  .instance_init = H264_instance_init,
};

void H264_init(void)
{
  Template_register(&H264_template);
}
#else
void H264_init(void)
{
  /* H264 module requires an updated x264 package, and I don't want to
     update and rebuild aria at this time. */
#warning dummy H264_init
}
#endif
