/* 
 * I don't like this module so much after all.  I  would rather have seperate
 * modules for different filters.  Only filters that operate in the same pass
 * should be in the same Template.  I suppose I could have several templates
 * in this file, implementing different filters.  They would show all up in
 * the template list.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_422P };
static Input VFilter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422p_handler },
};

enum { OUTPUT_422P };
static Output VFilter_outputs[] = {
  [ OUTPUT_422P ] = { .type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  int left_right_crop;
  int linear_blend;
} VFilter_private;

static int set_left_right_crop(Instance *pi, const char *value)
{
  VFilter_private *priv = pi->data;
  
  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid left_right_crop value %d\n", tmp);
    return -1;
  }
  else {
    priv->left_right_crop = tmp;
  }
  return 0;
}

static int set_linear_blend(Instance *pi, const char *value)
{
  VFilter_private *priv = pi->data;

  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid linear_blend value %d\n", tmp);
    return -1;
  }
  else {
    priv->linear_blend = tmp;
  }
  return 0;
}


static Config config_table[] = {
  { "left_right_crop",  set_left_right_crop, 0L, 0L },
  { "linear_blend",  set_linear_blend, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void single_121_linear_blend(uint8_t *data_in, uint8_t *data_out, int width, int height)
{
  int y, x;
  uint8_t *pPrev;
  uint8_t *p;
  uint8_t *pNext;
  uint8_t *dest = data_out;

  p = data_in;
  pNext = p + width;
  for (x=0; x < width; x++) {
    *dest = ((*p) * 2 + *pNext)/3;
    dest++;
    p++;
    pNext++;
  }

  pPrev = data_in;
  for (y=1; y < (height-1); y++) {
    for (x=0; x < width; x++) {
      *dest = (*pPrev + (*p) * 2 + *pNext)/4;
      dest++;
      pPrev++;
      p++;
      pNext++;
    }
  }

  for (x=0; x < width; x++) {
    *dest = (*pPrev + (*p) * 2)/3;
    dest++;
    pPrev++;
    p++;
  }
}


static void Y422p_handler(Instance *pi, void *msg)
{
  VFilter_private * priv = pi->data;
  Y422P_buffer *y422p_in = msg;
  Y422P_buffer *y422p_out = 0L;
  Y422P_buffer *y422p_src = 0L;
  
  if (priv->left_right_crop) {
    y422p_src = y422p_out ? y422p_out : y422p_in;
    if (priv->left_right_crop > y422p_src->width) {
      fprintf(stderr, "left_right_crop value %d is wider than input %d\n", 
	      priv->left_right_crop, y422p_src->width);
    }
    else {
      y422p_out = Y422P_buffer_new(y422p_src->width - (priv->left_right_crop * 2), y422p_src->height);
      memcpy(y422p_out->y, y422p_src->y+priv->left_right_crop, y422p_out->width);
      memcpy(y422p_out->cb, y422p_src->cb+(priv->left_right_crop/2), y422p_out->width/2);
      memcpy(y422p_out->cr, y422p_src->cr+(priv->left_right_crop/2), y422p_out->width/2);
      Y422P_buffer_discard(y422p_src);
    }
  }
  
  if (priv->linear_blend) {
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = Y422P_buffer_new(y422p_src->width, y422p_src->height);
    single_121_linear_blend(y422p_src->y, y422p_out->y, y422p_src->width, y422p_src->height);
    single_121_linear_blend(y422p_src->cb, y422p_out->cb, y422p_src->width/2, y422p_src->height);
    single_121_linear_blend(y422p_src->cr, y422p_out->cr, y422p_src->width/2, y422p_src->height);
    Y422P_buffer_discard(y422p_src);
  }
  
  if (!y422p_out) {
    y422p_out = y422p_in;
  }
  
  if (pi->outputs[OUTPUT_422P].destination) {
    PostData(y422p_out, pi->outputs[OUTPUT_422P].destination);
  }
  else {
    Y422P_buffer_discard(y422p_out);
  }
  
}


static void VFilter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void VFilter_instance_init(Instance *pi)
{
  VFilter_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template VFilter_template = {
  .label = "VFilter",
  .inputs = VFilter_inputs,
  .num_inputs = table_size(VFilter_inputs),
  .outputs = VFilter_outputs,
  .num_outputs = table_size(VFilter_outputs),
  .tick = VFilter_tick,
  .instance_init = VFilter_instance_init,
};

void VFilter_init(void)
{
  Template_register(&VFilter_template);
}
