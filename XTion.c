/* Search and replace "XTion" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "XTion.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input XTion_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output XTion_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  FILE *f;
  int frameno;
  int delayms;
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

static void process_frame(XTion_private * priv)
{
  uint16_t frame[320*240*sizeof(uint16_t)];
  if (!fread(frame, 320*240*sizeof(uint16_t), 1, priv->f)) {
    fclose(priv->f); priv->f = 0L;
    return;
  }
  priv->frameno++;
  printf("frame %d\n", priv->frameno++);
  if (priv->delayms) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = priv->delayms * 1000000}, NULL);
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
    process_frame(priv);
  }

  pi->counter++;
}

static void XTion_instance_init(Instance *pi)
{
  // XTion_private *priv = (XTion_private *)pi;
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
