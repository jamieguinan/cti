#include <stdio.h>
#include <string.h>
#include <stdlib.h>		/* abs */
#include "jpeglib.h"

#include "cdjpeg.h"
#include "wrmem.h"
#include "jmemsrc.h"

#include "CTI.h"
#include "MotionDetect.h"
#include "Images.h"
#include "MotionDetect.h"
#include "Mem.h"

static void Config_handler(Instance *pi, void *msg);
static void gray_handler(Instance *pi, void *msg);
static void mask_handler(Instance *pi, void *msg);

/* MotionDetect Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_GRAY, INPUT_MASK };
static Input MotionDetect_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY] = { .type_label = "GRAY_buffer", .handler = gray_handler },
  [ INPUT_MASK] = { .type_label = "MASK_buffer", .handler = mask_handler },
};

enum { OUTPUT_CONFIG, OUTPUT_MOTIONDETECT };
static Output MotionDetect_outputs[] = { 
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
  [ OUTPUT_MOTIONDETECT ] = { .type_label = "MotionDetect_result", .destination = 0L },
};

typedef struct {
  Gray_buffer *accum;
  Gray_buffer *mask;
  int running_sum;
  int remove_dc_offset;
} MotionDetect_private;


static int set_remove_dc_offset(Instance *pi, const char *value)
{
  MotionDetect_private *priv = pi->data;
  priv->remove_dc_offset = atoi(value);
  printf("remove_dc_offset set to %d\n", priv->remove_dc_offset);
  return 0;
}


static Config config_table[] = {
  { "remove_dc_offset", set_remove_dc_offset, 0L, 0L },
  /* ...Generic_set_int, offsetof(MotionDetect_private, remove_dc_offset) ... */
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void gray_handler(Instance *pi, void *msg)
{
  MotionDetect_private *priv = pi->data;
  Gray_buffer *gray = msg;
  int y, x, sum;
  int mask_check;

  dpf("%s:%s()\n", __FILE__, __func__);

  if (priv->accum &&
      (priv->accum->width != gray->width ||
       priv->accum->height != gray->height)) {
    /* Size has changed.  Discard accumulator buffer. */
    Gray_buffer_discard(priv->accum); priv->accum = 0L;
  }
		    
  if (!priv->accum) {
    priv->accum = Gray_buffer_new(gray->width, gray->height, 0L);
    memcpy(priv->accum->data, gray->data, gray->data_length);
  }

  if (priv->mask &&
      priv->mask->height == gray->height && 
      priv->mask->width == gray->width) {
    mask_check = 1;
  }
  else {
    mask_check = 0;
  }

  sum = 0;
  for (y=0; y < gray->height; y+=10) {
    for (x=0; x < gray->width; x+=10) {
      int offset = y * gray->width + x;

      if (mask_check) {
	if (priv->mask->data[offset] == 0) {
	  continue;
	}
      }

      /* The abs() is redundant with the squaring (d*d) below, but I'm
         keeping it anyway in case I remove or change the squaring. */
      int d = abs(priv->accum->data[offset] - gray->data[offset]); 
      sum += (d*d);

      /* Store with minimal IIR filtering. */
      priv->accum->data[offset] = (priv->accum->data[offset] * 2 +  gray->data[offset])/3;
    }
  }

  /* Running sum with more substantial IIR filtering. */
  priv->running_sum = (priv->running_sum * 15 + sum)/(15+1);

  if (pi->outputs[OUTPUT_CONFIG].destination) {
    char temp[64];
    snprintf(temp, 64, "%d %d", sum, priv->running_sum);
    PostData(Config_buffer_new("text", temp), pi->outputs[OUTPUT_CONFIG].destination);
  }

  if (pi->outputs[ OUTPUT_MOTIONDETECT].destination) {
    MotionDetect_result *md = MotionDetect_result_new();
    md->sum = sum;
    PostData(md, pi->outputs[ OUTPUT_MOTIONDETECT].destination);
  }

  Gray_buffer_discard(gray);
  // printf("MotionDetect sum=%d\n", sum);
}


static void mask_handler(Instance *pi, void *msg)
{
  MotionDetect_private *priv = pi->data;
  if (priv->mask) {
    Gray_buffer_discard(priv->mask);
  }
  priv->mask = msg;
}


static void MotionDetect_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void MotionDetect_instance_init(Instance *pi)
{
  MotionDetect_private *priv = Mem_calloc(1, sizeof(*priv));

  pi->data = priv;
}

static Template MotionDetect_template = {
  .label = "MotionDetect",
  .inputs = MotionDetect_inputs,
  .num_inputs = table_size(MotionDetect_inputs),
  .outputs = MotionDetect_outputs,
  .num_outputs = table_size(MotionDetect_outputs),
  .tick = MotionDetect_tick,
  .instance_init = MotionDetect_instance_init,
};

void MotionDetect_init(void)
{
  Template_register(&MotionDetect_template);
}


MotionDetect_result *MotionDetect_result_new(void)
{
  MotionDetect_result *md = Mem_calloc(1, sizeof(*md));
  return md;
}


void MotionDetect_result_discard(MotionDetect_result **md)
{
  Mem_free(*md);
  *md = 0L;
}
