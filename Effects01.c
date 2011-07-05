/* Search and replace "Effects01" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"

enum { INPUT_CONFIG, INPUT_RGB3, INPUT_422P };
static Input Effects01_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg" },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer" },
  [ INPUT_422P ] = { .type_label = "422P_buffer" },
};

enum { OUTPUT_RGB3, OUTPUT_422P };
static Output Effects01_outputs[] = {
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  uint8_t invert;
} Effects01_private;

static int set_invert(Instance *pi, const char *value)
{
  Effects01_private *priv = pi->data;
  if (atoi(value)) {
    priv->invert = 1;
  }
  else {
    priv->invert = 0;
  }
  return 0;
}

static Config config_table[] = {
  { "invert",   set_invert, 0L, 0L },
};

static void invert_plane(uint8_t *p, int length)
{
  int i;
  for (i=0; i < length; i++) {
    p[i] = 255 - p[i];
  }
}


static void Effects01_tick(Instance *pi)
{
  Effects01_private *priv = pi->data;
  int i;

  if (CheckMessage(pi, 1) == 0) {
    /* Weird... */
    fprintf(stderr, "%s: nothing to do!\n", __func__);
    return;
  }

  if (0) {
    Config_buffer *cb_in = PopMessage(&pi->inputs[INPUT_CONFIG]);

    /* Walk the config table. */
    for (i=0; i < table_size(config_table); i++) {
      if (streq(config_table[i].label, cb_in->label->bytes)) {
	int rc;		/* FIXME: What to do with this? */
	rc = config_table[i].set(pi, cb_in->value->bytes);
	break;
      }
    }
    
    Config_buffer_discard(&cb_in);
  }
  else if (0)  {
    RGB3_buffer *rgb3_in = PopMessage(&pi->inputs[INPUT_RGB3]);
    if (pi->outputs[OUTPUT_RGB3].destination) {
      /* Operate in-place, pass along to output... */
      if (priv->invert) {
	invert_plane(rgb3_in->data, rgb3_in->data_length);
      }
      PostData(rgb3_in, pi->outputs[OUTPUT_RGB3].destination);
    }
    else {
      RGB3_buffer_discard(rgb3_in);
    }
  }

  else if (0)  {
    Y422P_buffer *y422p_in = PopMessage(&pi->inputs[INPUT_422P]);
    if (pi->outputs[OUTPUT_422P].destination) {
      /* Operate in-place, pass along to output... */
      if (priv->invert) {
	invert_plane(y422p_in->y, y422p_in->y_length);
	invert_plane(y422p_in->cb, y422p_in->cb_length);
	invert_plane(y422p_in->cr, y422p_in->cr_length);
      }
      PostData(y422p_in, pi->outputs[OUTPUT_422P].destination);
    }
    else {
      Y422P_buffer_discard(y422p_in);
    }
  }


  
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
