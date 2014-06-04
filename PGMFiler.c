#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "PGMFiler.h"
#include "Images.h"
#include "Cfg.h"
#include "CTI.h"

static void Config_handler(Instance *pi, void *msg);
static void Gray_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_GRAY };
static Input PGMFiler_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY ] = { .type_label = "GRAY_buffer", .handler = Gray_handler },
};

//enum { /* OUTPUT_... */ };
static Output PGMFiler_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} PGMFiler_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Gray_handler(Instance *pi, void *msg)
{
  Gray_buffer *gray_in = msg;

  printf("gray_in @ %p\n", gray_in);

  if (pi->counter % 1 == 0) {
    char name[256];
    sprintf(name, "%09d.pgm", pi->counter);
    FILE *f = fopen(name, "wb");
    if (f) {
      fprintf(f, "P5\n%d %d\n255\n", gray_in->width, gray_in->height);
      if (fwrite(gray_in->data, gray_in->data_length, 1, f) != 1) {
	perror("fwrite");
      }
      fclose(f);
    }
  }

  /* Discard input buffer. */
  Gray_buffer_discard(gray_in);
  pi->counter += 1;
}


static void PGMFiler_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}


static void PGMFiler_instance_init(Instance *pi)
{
}

static Template PGMFiler_template = {
  .label = "PGMFiler",
  .priv_size = sizeof(PGMFiler_private),
  .inputs = PGMFiler_inputs,
  .num_inputs = table_size(PGMFiler_inputs),
  .outputs = PGMFiler_outputs,
  .num_outputs = table_size(PGMFiler_outputs),
  .tick = PGMFiler_tick,
  .instance_init = PGMFiler_instance_init,
};

void PGMFiler_init(void)
{
  Template_register(&PGMFiler_template);
}
