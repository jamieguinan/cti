#include <stdio.h>
#include <string.h>
#include <stdlib.h>		/* abs */
#include "jpeglib.h"

#include "cdjpeg.h"
#include "wrmem.h"
#include "jmemsrc.h"

#include "Template.h"
#include "MotionDetect.h"
#include "Images.h"
#include "MotionDetect.h"
#include "Mem.h"

static void Config_handler(Instance *pi, void *msg);
static void gray_handler(Instance *pi, void *msg);

/* MotionDetect Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_GRAY };
static Input MotionDetect_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY] = { .type_label = "GRAY_buffer", .handler = gray_handler },
};

enum { OUTPUT_CONFIG };
static Output MotionDetect_outputs[] = { 
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Gray_buffer *accum;
  int running_sum;
} MotionDetect_private;

static Config config_table[] = {
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

  if (priv->accum &&
      (priv->accum->width != gray->width ||
       priv->accum->height != gray->height)) {
    /* Size has changed.  Discard accumulator buffer. */
    Gray_buffer_discard(priv->accum); priv->accum = 0L;
  }
		    
  if (!priv->accum) {
    priv->accum = Gray_buffer_new(gray->width, gray->height);
  }

  sum = 0;
  for (y=0; y < gray->height; y+=10) {
    for (x=0; x < gray->width; x+=10) {
      int offset = y * gray->width + x;
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

  Gray_buffer_discard(gray);


  // printf("MotionDetect sum=%d\n", sum);
}

static void MotionDetect_tick(Instance *pi)
{
  /* Get motion score on past N gray frames. */
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
