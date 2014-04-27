/*
 * H264 encoder using libx264.
 * Note that libx264 is GPL, but a commercial license is available,
 *
 *   http://www.videolan.org/developers/x264.html
 * 
 * Notes on presets: https://trac.ffmpeg.org/wiki/x264EncodingGuide
 *
 * Additional references,
 *   http://mewiki.project357.com/wiki/X264_Settings
 *
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <inttypes.h>		/* PRIu32 */
#include <math.h>		/* round */
#include <unistd.h>		/* sleep */

#ifndef __tune_pentium4__

#include "CTI.h"
#include "H264.h"
#include "Images.h"
#include "x264.h"
//#include "output/output.h"
#include "SourceSink.h"

static void Config_handler(Instance *pi, void *msg);
static void YUV420P_handler(Instance *pi, void *msg);
static void YUV422P_handler(Instance *pi, void *msg);

/* x264.h says "nothing other than I420 is really supported", so use X264_CSP_I420 */
enum { INPUT_CONFIG, INPUT_YUV420P, INPUT_YUV422P };
static Input H264_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = YUV422P_handler },
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

  /* These next few values should point to x264_preset_*[] from x264.h */
  const char * preset;
  const char * tune;
  const char * profile;

  /* Similar to as used in x264.c: */
  //cli_output_t cli_output;
  //hnd_t hout;
  int rc_method;

  // huehuehuehuehuehue nobody fucking stops the hue

  int cqp;
  int crf;
  int keyint_max;		/* Generate a keyframe after this many input frames. */

  int processed_frames;
  int max_frames;		/* exit after this many frames if set */
} H264_private;


const char * nal_unit_type_e_strs[] = {
  [NAL_UNKNOWN] = "NAL_UNKNOWN",
  [NAL_SLICE] = "NAL_SLICE",
  [NAL_SLICE_DPA] = "NAL_SLICE_DPA",
  [NAL_SLICE_DPB] = "NAL_SLICE_DPB",
  [NAL_SLICE_DPC] = "NAL_SLICE_DPC",
  [NAL_SLICE_IDR] = "NAL_SLICE_IDR",
  [NAL_SEI] = "NAL_SEI",
  [NAL_SPS] = "NAL_SPS",
  [NAL_PPS] = "NAL_PPS",
  [NAL_AUD] = "NAL_AUD",
  [NAL_FILLER] = "NAL_FILLER",
};

#define nal_unit_type_e_str(i) ( i < 0 || i > NAL_FILLER ? "out-of-range" : nal_unit_type_e_strs[i])

const char * nal_priority_e_strs[] = {
  [NAL_PRIORITY_DISPOSABLE] = "NAL_PRIORITY_DISPOSABLE",
  [NAL_PRIORITY_LOW] = "NAL_PRIORITY_LOW",
  [NAL_PRIORITY_HIGH] = "NAL_PRIORITY_HIGH",
  [NAL_PRIORITY_HIGHEST] = "NAL_PRIORITY_HIGHEST",
};

#define nal_priority_e_str(i) (  i < 0 || i > NAL_PRIORITY_HIGHEST ? "out-of-range" : nal_priority_e_strs[i])


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


static int set_preset(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;

  int i;
  for (i=0; x264_preset_names[i]; i++) {
    if (streq(x264_preset_names[i], value)) {
      printf("H264: valid preset %s\n", x264_preset_names[i]);
      priv->preset = x264_preset_names[i];
      return 0;
    }
  }
  
  printf("H264: invalid preset %s\n", value);
  return 1;
}


static int set_tune(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;

  int i;
  for (i=0; x264_tune_names[i]; i++) {
    if (streq(x264_tune_names[i], value)) {
      printf("H264: valid tune %s\n", x264_tune_names[i]);
      priv->tune = x264_tune_names[i];
      return 0;
    }
  }
  
  printf("H264: invalid tune %s\n", value);
  return 1;
}


static int set_profile(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;

  int i;
  for (i=0; x264_profile_names[i]; i++) {
    if (streq(x264_profile_names[i], value)) {
      printf("H264: valid profile %s\n", x264_profile_names[i]);
      priv->profile = x264_profile_names[i];
      return 0;
    }
  }
  
  printf("H264: invalid profile %s\n", value);
  return 1;
}


static int set_cqp(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;
  priv->cqp = atoi(value);
  priv->crf = 0;
  return 0;
}


static int set_crf(Instance *pi, const char *value)
{
  H264_private *priv = (H264_private *)pi;
  priv->crf = atoi(value);
  priv->cqp = 0;
  return 0;
}


static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
  { "output",    set_output, 0L, 0L },
  { "preset",    set_preset, 0L, 0L },
  { "tune",      set_tune, 0L, 0L },
  { "profile",   set_profile, 0L, 0L },
  { "cqp",       set_cqp, 0L, 0L },
  { "crf",       set_crf, 0L, 0L },
  { "max_frames",0L, 0L, 0L, cti_set_int, offsetof(H264_private, max_frames) },
  { "keyint_max",0L, 0L, 0L, cti_set_int, offsetof(H264_private, keyint_max) },
};


static void show_x264_params(x264_param_t * params)
{
  printf("x264: i_fps_num = %" PRIu32 "\n", params->i_fps_num);
  printf("x264: i_fps_den = %" PRIu32 "\n", params->i_fps_den);
  printf("x264: i_timebase_num = %" PRIu32 "\n", params->i_timebase_num);
  printf("x264: i_timebase_den = %" PRIu32 "\n", params->i_timebase_den);
  printf("x264: b_aud = %d\n", params->b_aud);
  printf("x264: b_cabac = %d\n", params->b_cabac);
  printf("x264: b_vfr_input = %d\n", params->b_vfr_input);
  printf("x264: b_pulldown = %d\n", params->b_pulldown);
  printf("x264: b_annexb = %d\n", params->b_annexb);
  printf("x264: b_tff = %d\n", params->b_tff);
  printf("x264: b_pic_struct = %d\n", params->b_pic_struct);
  printf("x264: b_interlaced = %d\n", params->b_interlaced);
  printf("x264: b_fake_interlaced = %d\n", params->b_fake_interlaced);
  printf("x264: pf_log = %p\n", params->pf_log);
  printf("x264: analyse.i_me_method = %d\n", params->analyse.i_me_method);
  //printf("x264: analyze.i_me_method = %d\n", params->analyze.i_me_method);
  // printf("x264:  = %d\n", params->);
}


static void YUV420P_handler(Instance *pi, void *msg)
{
  int rc;
  int i;
  H264_private *priv = (H264_private *)pi;
  YUV420P_buffer *y420 = msg;

  if (!priv->encoder) {
    /* Set up the encoder and parameters. */
    x264_param_t params;

    x264_param_default(&params);

    rc = x264_param_default_preset(&params, priv->preset, priv->tune);
    if (rc != 0) { fprintf(stderr, "x264_param_default_preset failed"); sleep(1); return; }

    rc = x264_param_apply_profile(&params, priv->profile); 
    if (rc != 0) { fprintf(stderr, "x264_param_apply_profile failed");  sleep(1); return; }

    params.i_width = y420->width;
    params.i_height = y420->height;

    if (y420->c.fps_denominator) {
      params.i_fps_num = y420->c.fps_numerator;
      params.i_fps_den = y420->c.fps_denominator;
    }
    else if (y420->c.nominal_period > 0.0001) {
      params.i_fps_num = 10000;
      params.i_fps_den = (uint32_t)(round(10000*y420->c.nominal_period));
    }
    else {
      /* Default to 30fps */
      params.i_fps_num = 30000;
      params.i_fps_den = 1000;
    }

    /* Testing.  Aha!  This results in 29.97fps instead of 59.94fps playback in mplayer! */
    params.b_vfr_input = 0;

    /* Only enable this if needed for iOS playback. */
    params.b_aud = 1;

    /* Default motion estimation method is X264_ME_HEX, leave it at that. */
    //  params.analyse.i_me_method = X264_ME_UMH;
    /* FIXME: Enable this for satellite stream transcoding... */
    // params.b_tff = 1;

    /* Define keyframe interval, which will generate IDR frames and thus
     * allow propagation to MpegTSMux segmentation.  */
    if (priv->keyint_max) {
      params.i_keyint_max = priv->keyint_max;
    }
    else {
      /* 1/sec. */
      params.i_keyint_max = params.i_fps_num/params.i_fps_den;
    }

    /* b_intra_refresh disables IDR frames, so I don't want to set it. */
    //params.b_intra_refresh = 1;
    /* These don't seem to make much difference for my application... */
    //params.b_repeat_headers = 1;
    //params.b_annexb = 1;

    /* Which ever of cqp or crf is last set will be active. */
    if (priv->cqp) {
      params.rc.i_rc_method = X264_RC_CQP;
      params.rc.i_qp_constant = priv->cqp;
      //params.rc.i_qp_max = 0;
    }
    else if (priv->crf)  {
      params.rc.i_rc_method = X264_RC_CRF;
      params.rc.f_rf_constant = priv->crf;
    }
    else if (0)  {
      params.rc.i_rc_method = X264_RC_ABR;
      params.rc.i_bitrate = 1000000;
    }

    /* Show the parameters. */
    show_x264_params(&params);

    priv->encoder = x264_encoder_open(&params);
    if (!priv->encoder) { fprintf(stderr, "x264_encoder_open failed");  return; }
    printf("params fps=%d/%d\n", params.i_fps_num, params.i_fps_den);

    show_x264_params(&params);

    //priv->cli_output = mp4_output;
    //cli_output_opt_t output_opt = {};
    //rc = priv->cli_output.open_file("output.mp4", &priv->hout, &output_opt);
    //printf("cli_output.open_file returns %d\n", rc);
  }

  if (1) {
    x264_picture_t pic_in = {};
    x264_picture_t pic_out = {};
    x264_nal_t *nal = 0L;
    int pi_nal = 0;

    //if (pi->counter % 30 == 0) {
    //  pic_in.b_keyframe = 1;
    //}

    x264_picture_init(&pic_in);
    pic_in.img.i_csp = X264_CSP_I420;
    pic_in.img.i_plane = 3;
    pic_in.img.i_stride[0] = y420->width;
    pic_in.img.i_stride[1] = y420->cb_width;
    pic_in.img.i_stride[2] = y420->cr_width;
    pic_in.img.plane[0] = y420->y;
    pic_in.img.plane[1] = y420->cb;
    pic_in.img.plane[2] = y420->cr;

    /* FIXME: Set pts and dts correctly. */
    //pic_in.i_pts = priv->pts;
    //priv->pts += 1;

    int frame_size = x264_encoder_encode(priv->encoder, &nal, &pi_nal, &pic_in, &pic_out );

    dpf
    //printf
      ("counter=%d, frame_size=%d, %d nal units, pic.b_keyframe=%d, i_pts=%" PRIi64 " i_dts=%" PRIi64 "\n"
       , pi->counter
       , frame_size
       , pi_nal
       , pic_out.b_keyframe
       , pic_out.i_pts
       , pic_out.i_dts
       );

    for (i=0; i < pi_nal; i++) {
      //printf
      dpf
	("  nal[%d]: priority %s, type %s, startcode=%d, payloadsize=%d,\n",
	     i,
	     nal_priority_e_str(nal[i].i_ref_idc),
	     nal_unit_type_e_str(nal[i].i_type),
	     nal[i].b_long_startcode,
	     nal[i].i_payload);
    }
    
    //printf("\n");

    // "p_payload=%p i_payload=%d"
    // nal ? (nal->p_payload) : NULL,
    // nal ? (nal->i_payload) : 0
    
    if (0) {
      for (i=0; i < frame_size && i < 64;) {
	printf(" %02x", nal[0].p_payload[i]);
	i++;
	if (i % 16 == 0) {
	  printf("\n");
	}
      }
      printf("\n");
    }

    if (frame_size > 0 && pi->outputs[OUTPUT_H264].destination) {
      H264_buffer *hout = H264_buffer_from(nal[0].p_payload, frame_size, y420->width, y420->height, &y420->c);
      hout->keyframe = pic_out.b_keyframe;
      PostData(hout, pi->outputs[OUTPUT_H264].destination);
    }

    if (frame_size > 0 && sl(priv->output)) {
      Sink_write(priv->output_sink, nal[0].p_payload, frame_size);
    }

    if (frame_size > 0) {
      // i_frame_size = cli_output.write_frame( priv->hout, nal[0].p_payload, i_frame_size, &pic_out );
      // *last_dts = pic_out.i_dts;
    }

  }

  priv->processed_frames += 1;
  if (priv->max_frames && priv->max_frames == priv->processed_frames) {
    exit(0);
  }

  YUV420P_buffer_discard(y420);
}

static void YUV422P_handler(Instance *pi, void *msg)
{
  // int rc;
  /* FIXME:  Try setting "isp" (input color space) in the encoder instead
     of doing the conversion here. */
  YUV422P_buffer *y422 = msg;
  YUV420P_buffer *y420 = YUV422P_to_YUV420P(y422);
  YUV420P_handler(pi, y420);
  YUV422P_buffer_discard(y422);
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
  // H264_private *priv = (H264_private *)pi;
  set_preset(pi, "faster");
  set_tune(pi, "zerolatency");
  set_profile(pi, "baseline");
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
