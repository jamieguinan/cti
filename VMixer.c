/* Search and replace "VMixer" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "VMixer.h"
#include "Images.h"


static void Config_handler(Instance *pi, void *msg);
static void YUV422P_handler_1(Instance *pi, void *msg);
static void YUV422P_handler_2(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV422P_1, INPUT_YUV422P_2 };
static Input VMixer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P_1 ] = { .type_label = "YUV422P_buffer_1", .handler = YUV422P_handler_1 },
  [ INPUT_YUV422P_2 ] = { .type_label = "YUV422P_buffer_2", .handler = YUV422P_handler_2 },
};

enum { OUTPUT_YUV422P };
static Output VMixer_outputs[] = {
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  YUV422P_buffer * bufs[2];
  int mask;
  int width;
  int height;
  int xoffsets[2];
  int yoffset[2];
} VMixer_private;


static Config config_table[] = {
  //{ "size",    set_size, 0L, 0L },
  //{ "xoffset",  set_xoffset, 0L, 0L },
  //{ "yoffset",  set_yoffset, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void YUV422P_common_handler(Instance *pi, YUV422P_buffer *buffer, int index)
{
  VMixer_private *priv = (VMixer_private *)pi; 
  if (index >= 2) {
    /* Out of range. */
    return;
  }
  if (priv->bufs[index]) {
    YUV422P_buffer_discard(priv->bufs[index]);
  }
  priv->bufs[index] = buffer;
  priv->mask |= (1<<index);
  if (priv->mask == ((1<<2)-1)) {
    /* Full!  Combine and post. */
    YUV422P_buffer * out = YUV422P_buffer_new(priv->width, priv->height, NULL);
    memset(out->cb, 128, out->cb_length);
    memset(out->cr, 128, out->cr_length);
    
    int x, y, i, k;
    k=0;
    for (y=0; y < priv->height; y++) {
      for (x=0; x < priv->width; x++) {
	int ysum = 0;
	for (i=0; i < 2; i++) {
	  ysum += priv->bufs[i]->y[k];
	}
	out->y[k] = ysum/2;
	k++;
      }
    }

    k=0;
    for (y=0; y < priv->bufs[0]->cr_height; y++) {
      for (x=0; x < priv->bufs[0]->cr_width; x++) {
	int crsum = 0;
	int cbsum = 0;
	for (i=0; i < 2; i++) {
	  crsum += priv->bufs[i]->cr[k];
	  cbsum += priv->bufs[i]->cb[k];
	}
	out->cr[k] = crsum/2;
	out->cb[k] = cbsum/2;
	k++;
      }
    }

    priv->mask = 0;		/* reset */

    if (pi->outputs[OUTPUT_YUV422P].destination) {
      PostData(out, pi->outputs[OUTPUT_YUV422P].destination);
    }
  }
}


static void YUV422P_handler_1(Instance *pi, void *msg)
{
  YUV422P_buffer *buffer = msg;
  YUV422P_common_handler(pi, buffer, 0);
}


static void YUV422P_handler_2(Instance *pi, void *msg)
{
  YUV422P_buffer *buffer = msg;
  YUV422P_common_handler(pi, buffer, 1);
}


static void VMixer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void VMixer_instance_init(Instance *pi)
{
  VMixer_private *priv = (VMixer_private *)pi;
  priv->width = 640;
  priv->height = 480;
}


static Template VMixer_template = {
  .label = "VMixer",
  .priv_size = sizeof(VMixer_private),  
  .inputs = VMixer_inputs,
  .num_inputs = table_size(VMixer_inputs),
  .outputs = VMixer_outputs,
  .num_outputs = table_size(VMixer_outputs),
  .tick = VMixer_tick,
  .instance_init = VMixer_instance_init,
};

void VMixer_init(void)
{
  Template_register(&VMixer_template);
}
