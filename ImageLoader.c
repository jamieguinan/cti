#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "ImageLoader.h"
#include "Images.h"
#include "Array.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ImageLoader_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_GRAY, OUTPUT_422P };
static Output ImageLoader_outputs[] = {
  [ OUTPUT_GRAY ] = { .type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_422P ] = { .type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
} ImageLoader_private;


static int set_file(Instance *pi, const char *value)
{
  ArrayU8 *fdata = File_load_data(S((char*) value));
  ImageType t;
  Gray_buffer *gray = 0L;
  Y422P_buffer *y422p = 0L;

  if (!fdata) {
    fprintf(stderr, "failed to load %s\n", value);
    return 1;
  }

  /* Identify file. */
  t = Image_guess_type(fdata->data, fdata->len);

  if (t == IMAGE_TYPE_UNKNOWN) {
    if (strstr(value, ".y422p")) {
      t = IMAGE_TYPE_Y422P;
    }
  }

  switch (t) {
  case IMAGE_TYPE_PGM:
    gray = PGM_buffer_from(fdata->data, fdata->len, 0L);
    if (gray) {
      if (pi->outputs[OUTPUT_GRAY].destination) {
	PostData(gray, pi->outputs[OUTPUT_GRAY].destination);
      }
    }
    break;
  case IMAGE_TYPE_PPM:
    fprintf(stderr, "%s:%d PPM load not supported yet\n", __FILE__, __LINE__);
    break;
  case IMAGE_TYPE_JPEG:
    fprintf(stderr, "%s:%d JPEG load not supported yet\n", __FILE__, __LINE__);
    break;
  case IMAGE_TYPE_Y422P:
    y422p = Y422P_buffer_from(fdata->data, fdata->len, 0L);
    if (y422p) {
      if (pi->outputs[OUTPUT_GRAY].destination) {
	gray = Gray_buffer_from(y422p->y, y422p->width, y422p->height, 0L);
	PostData(gray, pi->outputs[OUTPUT_GRAY].destination);
      }
      if (pi->outputs[OUTPUT_422P].destination) {
	PostData(y422p, pi->outputs[OUTPUT_422P].destination);
      }
    }
    break;
  case IMAGE_TYPE_UNKNOWN:
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
