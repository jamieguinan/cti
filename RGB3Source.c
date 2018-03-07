/* Provide a single, possible blank, RGB3 buffer repeatedly, for
   animation or other purposes. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "RGB3Source.h"
#include "Images.h"
#include "File.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input RGB3Source_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  /* FIXME: Could accept an RGB3 input image, from ImageLoader. */
};

enum { OUTPUT_RGB3, OUTPUT_CONFIG };
static Output RGB3Source_outputs[] = { 
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  int width;
  int height;
  unsigned long color;
  int period_ms;	/* Frame repeat period. */
  RGB3_buffer *rgb3;
  int enable;
} RGB3Source_private;


static int generate_one_frame(Instance *pi)
{
  RGB3Source_private *priv = (RGB3Source_private *)pi;

  if (!priv->rgb3) {
    fprintf(stderr, "RGB3Source has no rgb3 data!\n");
    return 1;
  }

  if (!pi->outputs[OUTPUT_RGB3].destination) {
    fprintf(stderr, "RGB3Source has no destination!\n");
    return 1;
  }

  RGB3_buffer *tmp = RGB3_buffer_clone(priv->rgb3);
  PostData(tmp, pi->outputs[OUTPUT_RGB3].destination);

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  RGB3Source_private *priv = (RGB3Source_private *)pi;
  printf("%s:%s\n", __FILE__, __func__);

  if (!priv->width) {
    fprintf(stderr, "width not set\n");
    return 1;
  }
  if (!priv->height) {
    fprintf(stderr, "height not set\n");
    return 1;
  }
  if (!priv->rgb3) {
    priv->rgb3 = RGB3_buffer_new(priv->width, priv->height, NULL);
  }

  unsigned char r = (priv->color >> 16) & 0xff;
  unsigned char g = (priv->color >> 8) & 0xff;
  unsigned char b = (priv->color >> 0) & 0xff;

  int i;
  uint8_t *p = priv->rgb3->data;
  for (i=0; i < priv->rgb3->width *  priv->rgb3->height; i++) {
    *p++ = r;
    *p++ = g;
    *p++ = b;
  }

  priv->enable = 1;

  return 0;
}


static Config config_table[] = {
  // { "run",     do_run, 0L, 0L },
  { "color",   0L, 0L, 0L, cti_set_ulong, offsetof(RGB3Source_private, color) },
  { "width",   0L, 0L, 0L, cti_set_int, offsetof(RGB3Source_private, width) },
  { "height",   0L, 0L, 0L, cti_set_int, offsetof(RGB3Source_private, height) },
  { "period_ms", 0L, 0L, 0L, cti_set_int, offsetof(RGB3Source_private, period_ms) },
  { "enable",   set_enable, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


#if 0
static void rgb3_handler(Instance *pi, void *data)
{
  RGB3Source_private *priv = (RGB3Source_private *)pi;
  priv->rgb3 = data;
  printf("%s: rgb3=%p\n", __func__, priv->rgb3);
}
#endif


static void RGB3Source_tick(Instance *pi)
{
  RGB3Source_private *priv = (RGB3Source_private *)pi;
  Handler_message *hm;
  int wait_flag = (priv->enable ? 0 : 1);

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable) {
    return;
  }

  generate_one_frame(pi);

  int sec = priv->period_ms/1000;
  long nsec = (priv->period_ms % 1000) * 1000000;
  nanosleep(&(struct timespec) { .tv_sec = sec, .tv_nsec = nsec}, NULL);

  pi->counter++;
}


static void RGB3Source_instance_init(Instance *pi)
{
  RGB3Source_private *priv = (RGB3Source_private *)pi;
  /* Update once every 60 seconds.  The initial use for this is to drive the
   * text banner for satellite streams, and I am looking at getting SDL to
   * handle updates so that I don't have to feed from here, thus the long
   * periiod. */
  priv->period_ms = 60001/1;	
}


static Template RGB3Source_template = {
  .label = "RGB3Source",
  .priv_size = sizeof(RGB3Source_private),
  .inputs = RGB3Source_inputs,
  .num_inputs = table_size(RGB3Source_inputs),
  .outputs = RGB3Source_outputs,
  .num_outputs = table_size(RGB3Source_outputs),
  .tick = RGB3Source_tick,
  .instance_init = RGB3Source_instance_init,
};

void RGB3Source_init(void)
{
  Template_register(&RGB3Source_template);
}
