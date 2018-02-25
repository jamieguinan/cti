/*
 * Compose input images into output images.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Compositor.h"

static void Config_handler(Instance *pi, void *msg);
static void YUV420P_handler(Instance *pi, void *msg);

static int set_size(Instance *pi, const char *value);
static int set_require(Instance *pi, const char *value);
static int set_paste(Instance *pi, const char *value);

enum { INPUT_CONFIG, INPUT_YUV420P };
static Input Compositor_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
};

enum { OUTPUT_YUV420P };
static Output Compositor_outputs[] = {
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
};

typedef struct {
  String * label;
  struct {
    int x, y, w, h;
  } src;
  struct {
    int x, y;
  } dest;
  int rotate;
} PasteOp;


typedef struct {
  Instance i;
  int width;
  int height;
  String_list * required_labels;
  Array(PasteOp) operations;
} Compositor_private;

static Config config_table[] = {
  { "size",    set_size, NULL, NULL },
  { "require", set_require, NULL, NULL },
  { "paste",   set_paste, NULL, NULL },
};


static int set_size(Instance *pi, const char *value)
{
  return 0;
}

static int set_require(Instance *pi, const char *value)
{
  return 0;
}

static int set_paste(Instance *pi, const char *value)
{
  return 0;
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void YUV420P_handler(Instance *pi, void *msg)
{
}

static void Compositor_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Compositor_instance_init(Instance *pi)
{
  // Compositor_private *priv = (Compositor_private *)pi;
}


static Template Compositor_template = {
  .label = "Compositor",
  .priv_size = sizeof(Compositor_private),  
  .inputs = Compositor_inputs,
  .num_inputs = table_size(Compositor_inputs),
  .outputs = Compositor_outputs,
  .num_outputs = table_size(Compositor_outputs),
  .tick = Compositor_tick,
  .instance_init = Compositor_instance_init,
};

void Compositor_init(void)
{
  Template_register(&Compositor_template);
}

