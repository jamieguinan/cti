/*
 * Rotation, inversion, and other simple operations. 
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Y420p_handler(Instance *pi, void *msg);
static void RGB3_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_YUV420P, INPUT_RGB3 };
static Input Effects01_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = Y420p_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
};

enum { OUTPUT_YUV420P, OUTPUT_RGB3 };
static Output Effects01_outputs[] = {
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
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


static void rotate_single_90(uint8_t *src, uint8_t *dst, int src_width, int src_height)
{
  int src_x, src_y;
  int dst_width = src_height;
  int dst_height = src_width;
  for (src_y = 0; src_y < src_height; src_y++) {
    uint8_t *p_in = src + src_y*src_width;
    uint8_t *p_out = dst + (dst_width * (dst_height - 1) + src_y);
    for (src_x = 0; src_x < src_width; src_x++) {
      *p_out++ = *p_in++;
      p_out -= 1;
      p_out -= dst_width;
    }
  }
}


static void rotate_single_270(uint8_t *src, uint8_t *dst, int src_width, int src_height)
{
  int src_x, src_y;
  int dst_width = src_height;
  int dst_height = src_width;
  for (src_y = 0; src_y < src_height; src_y++) {
    uint8_t *p_in = src + src_y*src_width;
    uint8_t *p_out = dst + (dst_width * (dst_height - 1) + src_y);
    for (src_x = 0; src_x < src_width; src_x++) {
      *p_out++ = *p_in++;
      p_out -= 1;
      p_out -= dst_width;
    }
  }
}


static void Y420p_handler(Instance *pi, void *data)
{
  Effects01_private *priv = (Effects01_private *)pi;
  YUV420P_buffer *y420p = data;

  if (!pi->outputs[OUTPUT_YUV420P].destination) {
    YUV420P_buffer_discard(y420p);
    return;
  }

  if (priv->invert) {
    invert_plane(y420p->y, y420p->y_length);
  }

  if (priv->rotate == 90) {
    YUV420P_buffer *y420p_new = YUV420P_buffer_new(y420p->height, y420p->width, &y420p->c);
    rotate_single_90(y420p->y, y420p_new->y, y420p->width, y420p->height);
    rotate_single_90(y420p->cr, y420p_new->cr, y420p->cr_width, y420p->cr_height);
    rotate_single_90(y420p->cb, y420p_new->cb, y420p->cb_width, y420p->cb_height);
    YUV420P_buffer_discard(y420p);
    y420p = y420p_new;
  }

  if (priv->rotate == 270) {
    YUV420P_buffer *y420p_new = YUV420P_buffer_new(y420p->height, y420p->width, &y420p->c);
    rotate_single_270(y420p->y, y420p_new->y, y420p->width, y420p->height);
    rotate_single_270(y420p->cr, y420p_new->cr, y420p->cr_width, y420p->cr_height);
    rotate_single_270(y420p->cb, y420p_new->cb, y420p->cb_width, y420p->cb_height);
    YUV420P_buffer_discard(y420p);
    y420p = y420p_new;
  }

  PostData(y420p, pi->outputs[OUTPUT_YUV420P].destination);
}


static void RGB3_handler(Instance *pi, void *data)
{
  Effects01_private *priv = (Effects01_private *)pi;
  RGB3_buffer *rgb3_in = data;

  if (!pi->outputs[OUTPUT_RGB3].destination) {
    RGB3_buffer_discard(rgb3_in);
    return;
  }

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


static void Effects01_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Effects01_instance_init(Instance *pi)
{
  // Effects01_private *priv = (Effects01_private *)pi;
}


static Template Effects01_template = {
  .label = "Effects01",
  .priv_size = sizeof(Effects01_private),
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
