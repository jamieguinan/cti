/*
 * Load binary image files of various types, and post to connected
 * modules.  Posts are synchronous to allow feeding a succession of
 * source image files without overwhelming the destination inputs.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "ImageLoader.h"
#include "Images.h"
#include "ArrayU8.h"
#include "File.h"
#include "Audio.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ImageLoader_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_GRAY, OUTPUT_YUV422P, OUTPUT_YUV420P, OUTPUT_JPEG, OUTPUT_H264, OUTPUT_AAC,
       OUTPUT_RGB3 };
static Output ImageLoader_outputs[] = {
  [ OUTPUT_GRAY ] = { .type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_H264 ] = { .type_label = "H264_buffer", .destination = 0L },
  [ OUTPUT_AAC ] = { .type_label = "AAC_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
} ImageLoader_private;


static int set_file(Instance *pi, const char *value)
{
  ArrayU8 *fdata = File_load_data(S((char*) value));
  ImageType t;
  Gray_buffer *gray = 0L;
  YUV422P_buffer *y422p = 0L;
  // YUV420P_buffer *y420p = 0L;
  Jpeg_buffer *jpeg = 0L;
  H264_buffer *h264 = 0L;
  AAC_buffer *aac = 0L;
  RGB3_buffer *rgb = 0L;
  int result;

  printf("fdata->len=%ld\n", fdata->len);

  if (!fdata) {
    fprintf(stderr, "failed to load %s\n", value);
    return 1;
  }

  /* Identify file. */
  t = Image_guess_type(fdata->data, fdata->len);

  if (t == IMAGE_TYPE_UNKNOWN) {
    if (strstr(value, ".y422p")) {
      t = IMAGE_TYPE_YUV422P;
    }
  }
  
  switch (t) {
  case IMAGE_TYPE_PGM:
    gray = PGM_buffer_from(fdata->data, fdata->len, 0L);
    if (gray) {
      if (pi->outputs[OUTPUT_GRAY].destination) {
	PostDataGetResult(gray, pi->outputs[OUTPUT_GRAY].destination, &result);
      }
      else {
	// discard
      }
    }
    break;
  case IMAGE_TYPE_PPM:
    rgb = PPM_buffer_from(fdata->data, fdata->len, 0L);
    printf("rgb=%p\n", rgb);
    if (rgb) {
      if (pi->outputs[OUTPUT_RGB3].destination) {
	printf("posting...\n");
	PostDataGetResult(rgb, pi->outputs[OUTPUT_RGB3].destination, &result);
      }
      else {
	// discard
      }
    }
    break;
  case IMAGE_TYPE_JPEG:
    jpeg = Jpeg_buffer_from(fdata->data, fdata->len, 0L);
    if (jpeg) {
      if (pi->outputs[OUTPUT_JPEG].destination) {
	PostDataGetResult(jpeg, pi->outputs[OUTPUT_JPEG].destination, &result);
      }
      else {
	// discard
      }
    }
    break;
  case IMAGE_TYPE_YUV422P:
    // y422p = ...
    if (y422p) {
      if (pi->outputs[OUTPUT_GRAY].destination) {
	gray = Gray_buffer_from(y422p->y, y422p->width, y422p->height, 0L);
	PostDataGetResult(gray, pi->outputs[OUTPUT_GRAY].destination, &result);
	// Gray_buffer_discard();
      }
      if (pi->outputs[OUTPUT_YUV422P].destination) {
	PostDataGetResult(y422p, pi->outputs[OUTPUT_YUV422P].destination, &result);
      }
      else {
	// discard
      }
    }
    break;
  case IMAGE_TYPE_H264:
    //printf("H264...\n");
    h264 = H264_buffer_from(fdata->data, fdata->len, 0, 0, NULL);
    if (h264) {
      if (pi->outputs[OUTPUT_H264].destination) {
	PostDataGetResult(h264, pi->outputs[OUTPUT_H264].destination, &result);
	// printf("H264 result=%d\n", result);
      }
    }
    break;
  case AUDIO_TYPE_AAC:
    aac = AAC_buffer_from(fdata->data, fdata->len);
    if (aac) {
      if (pi->outputs[OUTPUT_AAC].destination) {
	PostDataGetResult(aac, pi->outputs[OUTPUT_AAC].destination, &result);
	// printf("AAC result=%d\n", result);
      }
      else {
	// discard
      }
    }
    break;
  case IMAGE_TYPE_UNKNOWN:
    printf("Could not not identify file type for %s\n", value);
  default:
    break;
  }

  ArrayU8_cleanup(&fdata);
  return 0;
}


static Config config_table[] = {
  { "file",    set_file, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void ImageLoader_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void ImageLoader_instance_init(Instance *pi)
{
  // ImageLoader_private *priv = (ImageLoader_private *)pi;
}


static Template ImageLoader_template = {
  .label = "ImageLoader",
  .priv_size = sizeof(ImageLoader_private),
  .inputs = ImageLoader_inputs,
  .num_inputs = table_size(ImageLoader_inputs),
  .outputs = ImageLoader_outputs,
  .num_outputs = table_size(ImageLoader_outputs),
  .tick = ImageLoader_tick,
  .instance_init = ImageLoader_instance_init,
};

void ImageLoader_init(void)
{
  Template_register(&ImageLoader_template);
}
