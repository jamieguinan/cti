/* Provide a single jpeg file repeatedly, for benchmarking. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "JpegSource.h"
#include "Images.h"
#include "File.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input JpegSource_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_JPEG, OUTPUT_CONFIG };
static Output JpegSource_outputs[] = { 
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  Jpeg_buffer *jpeg;
} JpegSource_private;


static int set_file(Instance *pi, const char *value)
{
  JpegSource_private *priv = (JpegSource_private *)pi;
  ArrayU8 *fdata = File_load_data(value);
  if (fdata) {
    priv->jpeg = Jpeg_buffer_from(fdata->data, fdata->len, 0L);
  }
  else {
    fprintf(stderr, "failed to load %s\n", value);
  }

  ArrayU8_cleanup(&fdata);
  return 0;
}


static int do_run(Instance *pi, const char *value)
{
  /* Clones and posts the jpeg buffer N times.  N=atoi(value). */
  JpegSource_private *priv = (JpegSource_private *)pi;
  int count = atoi(value);
  int i;

  if (!priv->jpeg) {
    fprintf(stderr, "JpegSource has no jpeg data!\n");
    return 1;
  }

  if (!pi->outputs[OUTPUT_JPEG].destination) {
    fprintf(stderr, "JpegSource has no destination!\n");
    return 1;
  }

  for (i=0; i < count; i++) {
    Jpeg_buffer *tmp = Jpeg_buffer_from(priv->jpeg->data, priv->jpeg->encoded_length, 0L);
    if (cfg.verbosity) {
      printf("%d/%d (%d)\n", i, count,
	     pi->outputs[OUTPUT_JPEG].destination->parent->pending_messages);
    }

    while (pi->outputs[OUTPUT_JPEG].destination->parent->pending_messages > 5) {
      /* Throttle output.  25ms sleep. */
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 25 * 1000 * 1000}, NULL);
    }

    PostData(tmp, pi->outputs[OUTPUT_JPEG].destination);
  }

  if (pi->outputs[OUTPUT_CONFIG].destination) {
    PostData(Config_buffer_new("quit", "0"), pi->outputs[OUTPUT_CONFIG].destination);
  }

  return 0;
}


static Config config_table[] = {
  { "file",    set_file, 0L, 0L },
  { "run",     do_run, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void JpegSource_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void JpegSource_instance_init(Instance *pi)
{
  // JpegSource_private *priv = (JpegSource_private *)pi;
}


static Template JpegSource_template = {
  .label = "JpegSource",
  .inputs = JpegSource_inputs,
  .num_inputs = table_size(JpegSource_inputs),
  .outputs = JpegSource_outputs,
  .num_outputs = table_size(JpegSource_outputs),
  .tick = JpegSource_tick,
  .instance_init = JpegSource_instance_init,
};

void JpegSource_init(void)
{
  Template_register(&JpegSource_template);
}
