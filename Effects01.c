#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void RGB3_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_RGB3 };
static Input Effects01_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
};

enum { OUTPUT_RGB3 };
static Output Effects01_outputs[] = {
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};

typedef struct {
  uint8_t invert;
  int rotate;
} Effects01_private;


static Config config_table[] = {
  { "invert",   0L, 0L, 0L, cti_set_int, offsetof(Effects01_private, invert) },
  { "rotate",   0L, 0L, 0L, cti_set_int, offsetof(Effects01_private, rotate) },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void invert_plane(uint8_t *p, int length)
{
  int i;
  for (i=0; i < length; i++) {
    p[i] = 255 - p[i];
  }
}


static void rotate_rgb_90(uint8_t *src, uint8_t *dst, int src_width, int src_height)
{
  int src_x, src_y;
  int dst_width = src_height;
  for (src_y = 0; src_y < src_height; src_y++) {
    uint8_t *p_in = src + src_y*src_width*3;
    uint8_t *p_out = dst + (dst_width - 1 - src_y)*3;
    for (src_x = 0; src_x < src_width; src_x++) {
      *p_out++ = *p_in++; 
      *p_out++ = *p_in++; 
      *p_out++ = *p_in++;
      p_out += (dst_width*3 - 3);
    }
  }
}


static void rotate_rgb_270(uint8_t *src, uint8_t *dst, int src_width, int src_height)
{
  int src_x, src_y;
  int dst_width = src_height;
  int dst_height = src_width;
  for (src_y = 0; src_y < src_height; src_y++) {
    uint8_t *p_in = src + src_y*src_width*3;
    uint8_t *p_out = dst + (dst_width * (dst_height - 1) + src_y) * 3;
    for (src_x = 0; src_x < src_width; src_x++) {
      *p_out++ = *p_in++; 
      *p_out++ = *p_in++; 
      *p_out++ = *p_in++;
      p_out -= 3;
      p_out -= dst_width*3;
    }
  }
}


static void RGB3_handler(Instance *pi, void *data)
{
  Effects01_private *priv = pi->data;
  RGB3_buffer *rgb3_in = data;
  if (pi->outputs[OUTPUT_RGB3].destination) {
    /* Operate in-place, pass along to output... */
    if (priv->invert) {
      invert_plane(rgb3_in->data, rgb3_in->data_length);
    }
    if (priv->rotate == 90) {
      RGB3_buffer *rgb3_new = RGB3_buffer_new(rgb3_in->height, rgb3_in->width, &rgb3_in->c);
      rotate_rgb_90(rgb3_in->data, rgb3_new->data, rgb3_in->width, rgb3_in->height);
      RGB3_buffer_discard(rgb3_in);
      rgb3_in = rgb3_new;
    }
    if (priv->rotate == 270) {
      RGB3_buffer *rgb3_new = RGB3_buffer_new(rgb3_in->height, rgb3_in->width, &rgb3_in->c);
      rotate_rgb_270(rgb3_in->data, rgb3_new->data, rgb3_in->width, rgb3_in->height);
      RGB3_buffer_discard(rgb3_in);
      rgb3_in = rgb3_new;
    }
    PostData(rgb3_in, pi->outputs[OUTPUT_RGB3].destination);
  }
  else {
    RGB3_buffer_discard(rgb3_in);
  }

}


static void Effects01_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void Effects01_instance_init(Instance *pi)
{
  Effects01_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template Effects01_template = {
  .label = "Effects01",
  .inputs = Effects01_inputs,
  .num_inputs = table_size(Effects01_inputs),
  .outputs = Effects01_outputs,
  .num_outputs = table_size(Effects01_outputs),
  .tick = Effects01_tick,
  .instance_init = Effects01_instance_init,
};

void Effects01_init(void)
{
  Template_register(&Effects01_template);
}
