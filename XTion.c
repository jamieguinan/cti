/* Search and replace "XTion" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "XTion.h"
#include "Images.h"
#include "Keycodes.h"

static void Config_handler(Instance *pi, void *msg);
static void Keycode_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input XTion_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_GRAY };
static Output XTion_outputs[] = {
[ OUTPUT_GRAY ] = {.type_label = "GRAY_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  FILE *f;
  int frameno;
  int delayms;
  int paused;
  struct {
    int x;
    int y;
    int width;
    int height;
  } roi;
} XTion_private;

static int set_file(Instance *pi, const char *value)
{
  XTion_private *priv = (XTion_private *)pi;
  priv->f = fopen(value, "rb");
  if (priv->f) {
    printf("opened %s\n", value);
  }
  return 0;
}

static Config config_table[] = {
  { "file",   set_file, 0L, 0L },
  { "delayms",  0L, 0L, 0L, cti_set_int, offsetof(XTion_private, delayms)},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Keycode_handler(Instance *pi, void *msg)
{
  XTion_private *priv = (XTion_private *)pi;
  Keycode_message *km = msg;

  if (km->keycode == CTI__KEY_SPACE) {
    priv->paused = priv->paused ? 0 : 1;
  }

  Keycode_message_cleanup(&km);
}


static int image_width = 320;
static int image_height = 240;


void gray_draw_box(Gray_buffer *gb, int x, int y, int width, int height, uint8_t value)
{
  int ix, iy;
  
  for (iy=y; iy < (y+height); iy++) {
    if (iy < 0 || iy >= gb->height) continue;
    uint8_t * p = &(gb->data[gb->width * iy]);
    if (iy == y || iy == ((y+height-1))) {
      /* Draw line. */
      for (ix=x; ix <= (x+width); ix++) {
	if (ix > 0 && ix < (gb->width)) {
	  p[ix] = value;
	}
      }
    }
    else {
      /* Draw edge points. */
      ix = x+width;
      if (x > 0 && x < (gb->width)) {  p[x] = value; }
      if (ix > 0 && ix < (gb->width)) {  p[ix] = value; }
    }
  }
}


static void touch_detect(XTion_private *priv, Gray_buffer *gray)
{
  
}


static void process_frame(Instance *pi)
{
  XTion_private *priv = (XTion_private *)pi;
  uint16_t frame[image_width*image_height*sizeof(uint16_t)];

  //  printf("frame %d\n", priv->frameno);
  if (priv->delayms) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (1000000000/60)}, NULL);
  }

  if (priv->paused) {
    return;
  }

  if (!fread(frame, image_width*image_height*sizeof(uint16_t), 1, priv->f)) {
    fclose(priv->f); priv->f = 0L;
    return;
  }
  priv->frameno++;

  if (pi->outputs[OUTPUT_GRAY].destination) {
    /* Create gray buffer. */
    int z;
    Gray_buffer *gray_out = Gray_buffer_new(image_width, image_height, NULL);
  
    for (z=0; z < (image_width*image_height); z++) {
      gray_out->data[z] = ((frame[z] - 1000)/6)&0xff;
    }

    if (priv->roi.width) {
      gray_draw_box(gray_out, priv->roi.x-1, priv->roi.y-1, priv->roi.width+2, priv->roi.height+2, 255);
      touch_detect(priv, gray_out);
      gray_draw_box(gray_out, priv->roi.x-1, priv->roi.y-1, priv->roi.width+2, priv->roi.height+2, 255);
    }

    PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
  }
}


static void XTion_tick(Instance *pi)
{
  XTion_private *priv = (XTion_private *)pi;

  Handler_message *hm;
  int wait_flag;
  if (priv->f) {
    wait_flag = 0;
  }
  else {
    wait_flag = 1;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (priv->f) {
    process_frame(pi);
  }

  pi->counter++;
}

static void XTion_instance_init(Instance *pi)
{
  XTion_private *priv = (XTion_private *)pi;
  priv->roi.x = 20;
  priv->roi.y = 20;
  priv->roi.width = 200;
  priv->roi.height = 160;
}


static Template XTion_template = {
  .label = "XTion",
  .priv_size = sizeof(XTion_private),  
  .inputs = XTion_inputs,
  .num_inputs = table_size(XTion_inputs),
  .outputs = XTion_outputs,
  .num_outputs = table_size(XTion_outputs),
  .tick = XTion_tick,
  .instance_init = XTion_instance_init,
};

void XTion_init(void)
{
  Template_register(&XTion_template);
}
