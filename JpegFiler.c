#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "JpegFiler.h"
#include "Images.h"
#include "Cfg.h"
#include "CTI.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input JpegFiler_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

//enum { /* OUTPUT_... */ };
static Output JpegFiler_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  int fnumber;
  String * prefix;
  FILE * notify_fifo;
} JpegFiler_private;

static int set_prefix(Instance *pi, const char *value)
{
  JpegFiler_private * priv = (JpegFiler_private *)pi;
  String_set(&priv->prefix, value);
  return 0;
}

static int set_notify_fifo(Instance *pi, const char *value)
{
  JpegFiler_private * priv =  (JpegFiler_private *) pi;
  priv->notify_fifo = fopen(value, "w");
  printf("notify_fifo(%s) = %p\n", value, priv->notify_fifo);
  return 0;
}


static Config config_table[] = {
  { "prefix",    set_prefix, 0L, 0L},
  { "notify_fifo",    set_notify_fifo, 0L, 0L},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Jpeg_handler(Instance *pi, void *msg)
{
  JpegFiler_private *priv = (JpegFiler_private *) pi;
  Jpeg_buffer *jpeg_in = msg;
  char name[256];

  while (1) {
    priv->fnumber += 1;
    sprintf(name, "%s%09d.jpg",
	    priv->prefix ? s(priv->prefix) : "",
	    priv->fnumber);
    if (File_exists(S(name))) {
      continue;
    }
    else {
      break;
    }
  }


  FILE *f = fopen(name, "wb");
  if (f) {
    if (fwrite(jpeg_in->data, jpeg_in->encoded_length, 1, f) != 1) {
      perror("fwrite");
    }
    fclose(f);
  }

  if (jpeg_in->c.eof) {
    fprintf(stderr, "%s detected EOF\n", __func__);
    exit(0);
  }

  if (priv->notify_fifo) {
    fprintf(priv->notify_fifo, "%s\n", name);
  }

  /* Discard input buffer. */
  Jpeg_buffer_release(jpeg_in);
  pi->counter += 1;
}


static void JpegFiler_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}


static void JpegFiler_instance_init(Instance *pi)
{
}

static Template JpegFiler_template = {
  .label = "JpegFiler",
  .priv_size = sizeof(JpegFiler_private),
  .inputs = JpegFiler_inputs,
  .num_inputs = table_size(JpegFiler_inputs),
  .outputs = JpegFiler_outputs,
  .num_outputs = table_size(JpegFiler_outputs),
  .tick = JpegFiler_tick,
  .instance_init = JpegFiler_instance_init,
};

void JpegFiler_init(void)
{
  Template_register(&JpegFiler_template);
}
