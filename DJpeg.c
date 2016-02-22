/* 
 * Jpeg decompression module.
 */
#include <stdio.h>
#include <stdlib.h>		/* exit */
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

#include "jpeghufftables.h"

#include "CTI.h"
#include "DJpeg.h"
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"
#include "ArrayU8.h"

#include "jpeg_misc.h"

static void Config_handler(Instance *pi, void *data);
static void Jpeg_handler(Instance *pi, void *data);

/* DJpeg Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_JPEG };
static Input DJpeg_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

enum { OUTPUT_RGB3, OUTPUT_GRAY, OUTPUT_YUV422P, OUTPUT_YUV420P, OUTPUT_JPEG };
static Output DJpeg_outputs[] = { 
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = {.type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = {.type_label = "Jpeg_buffer", .destination = 0L }, /* pass-through */
};

typedef struct {
  Instance i;

  /* Jpeg decode context... */
  int use_green_for_gray;
  int sampling_warned;
  int max_messages;
  int dct_method;

  int every;
} 
DJpeg_private;


static int set_dct_method(Instance *pi, const char *value)
{
  DJpeg_private *priv = (DJpeg_private *)pi;

  if (streq(value, "islow")) {
    priv->dct_method = JDCT_ISLOW;
  }
  else if (streq(value, "ifast")) {
    priv->dct_method = JDCT_IFAST;
  }
  else if (streq(value, "float")) {
    priv->dct_method = JDCT_FLOAT;
  }
  else {
    fprintf(stderr, "%s: unknown method %s\n", __func__, value);
  }
  return 0;
}


static int do_quit(Instance *pi, const char *value)
{
  exit(0);
  return 0;
}


static Config config_table[] = {
  { "max_messages", 0L, 0L, 0L, cti_set_int, offsetof(DJpeg_private, max_messages) },
  { "every", 0L, 0L, 0L, cti_set_int, offsetof(DJpeg_private, every) },
  { "dct_method", set_dct_method, 0L, 0L},
  { "quit",    do_quit, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Jpeg_handler(Instance *pi, void *data)
{
  DJpeg_private *priv = (DJpeg_private *)pi;
  double t1, t2;
  int save_width = 0;
  int save_height = 0;
  Jpeg_buffer *jpeg_in = data;

  /* Provisional image buffers. */
  YUV420P_buffer * yuv420p = NULL;
  YUV422P_buffer * yuv422p = NULL;
  RGB3_buffer * rgb3 = NULL;

  if (priv->every && (pi->counter % priv->every != 0)) {
    goto out;
  }

  if (priv->max_messages && pi->pending_messages > priv->max_messages) {
    /* Skip without decoding. */
    fprintf(stderr, "DJpeg skipping %d (%d %d)\n", pi->counter,
	    pi->pending_messages, priv->max_messages);
    goto out;
  }

  getdoubletime(&t1);

  if (!(pi->outputs[OUTPUT_YUV422P].destination ||
	pi->outputs[OUTPUT_YUV420P].destination ||
	pi->outputs[OUTPUT_RGB3].destination ||
	pi->outputs[OUTPUT_GRAY].destination)) {
    /* No decompressed outputs set up. */
    goto out;
  }

  FormatInfo * fmt = NULL;

  Jpeg_decompress(jpeg_in, &yuv420p, &yuv422p, &fmt);

  /* Sanity check... */
  if (!yuv422p && !yuv420p) {
    printf("neither yuv422p or yuv420p is set!?\n");
    goto out;
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    /* Clone Y channel, pass along. */
    if (yuv422p) {
      Gray_buffer *gray_out = Gray_buffer_new(yuv422p->width, yuv422p->height, &jpeg_in->c);
      memcpy(gray_out->data, yuv422p->y, yuv422p->y_length);
      PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
    }
    else if (yuv420p) {
      Gray_buffer *gray_out = Gray_buffer_new(yuv420p->width, yuv420p->height, &jpeg_in->c);
      memcpy(gray_out->data, yuv420p->y, yuv420p->y_length);
      PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
    }
  }

  if (pi->outputs[OUTPUT_YUV422P].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV420P && !yuv422p) {
      /* Convert. */
      yuv422p = YUV420P_to_YUV422P(yuv420p);
    }
    /* Post. */
    PostData(YUV422P_buffer_ref(yuv422p), pi->outputs[OUTPUT_YUV422P].destination);
  }

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV422P && !yuv420p) {
      /* Convert. */
      yuv420p = YUV422P_to_YUV420P(yuv422p);
    }
    /* Post. */
    PostData(YUV420P_buffer_ref(yuv420p), pi->outputs[OUTPUT_YUV420P].destination);
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV420P) {
      rgb3 = YUV420P_to_RGB3(yuv420p);
    }
    else if (fmt->imgtype == IMAGE_TYPE_YUV422P) {
      rgb3 = YUV422P_to_RGB3(yuv422p);
    }
    /* Post... */
    PostData(RGB3_buffer_ref(rgb3), pi->outputs[OUTPUT_RGB3].destination);
  }

  /* Discard/unref buffers. */
  if (yuv422p) {
    YUV422P_buffer_discard(yuv422p);
  }

  if (yuv420p) {
    YUV420P_buffer_discard(yuv420p);
  }

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }

 out:
  /* Discard or pass along input buffer. */
  if (pi->outputs[OUTPUT_JPEG].destination) {
    PostData(jpeg_in, pi->outputs[OUTPUT_JPEG].destination);
  }
  else {
    Jpeg_buffer_discard(jpeg_in);
  }
  pi->counter += 1;

  /* Calculate decompress time. */
  getdoubletime(&t2);
  float tdiff = t2 - t1;

  dpf("djpeg %.5f (%dx%d)\n", tdiff, save_width, save_height);
}

static void DJpeg_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}

static void DJpeg_instance_init(Instance *pi)
{
  DJpeg_private *priv = (DJpeg_private *)pi;
  priv->use_green_for_gray = 1;
  priv->dct_method = JDCT_DEFAULT;
}

static Template DJpeg_template = {
  .label = "DJpeg",
  .priv_size = sizeof(DJpeg_private),
  .inputs = DJpeg_inputs,
  .num_inputs = table_size(DJpeg_inputs),
  .outputs = DJpeg_outputs,
  .num_outputs = table_size(DJpeg_outputs),
  .tick = DJpeg_tick,
  .instance_init = DJpeg_instance_init,
};


void DJpeg_init(void)
{
  Template_register(&DJpeg_template);
}
