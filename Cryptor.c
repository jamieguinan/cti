/* ... */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "Cryptor.h"
#include "Array.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_RAWDATA };
static Input Cryptor_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

// enum { OUTPUT_RAWDATA };
static Output Cryptor_outputs[] = {
};

typedef struct {
  Cryptor_context ctx;
} Cryptor_private;

static int set_cipher(Instance *pi, const char *value)
{
  Cryptor_private *priv = pi->data;

  Cryptor_context_init(&priv->ctx, String_new(value));

  return 0;
}

static Config config_table[] = {
  { "cipher", set_cipher, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

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

void Cryptor_context_init(Cryptor_context *ctx, String *filename)
{
  ctx->cipher = File_load_data(filename->bytes);
  if (!ctx->cipher) {
    fprintf(stderr, "failed to load cipher file %s\n", filename->bytes);
  }
  else {
    fprintf(stderr, "loaded cipher block from %s (%d bytes)\n", filename->bytes, ctx->cipher->len);
  }
}


void Cryptor_process_data(Cryptor_context *ctx, unsigned int *cipher_index, uint8_t *data, int len)
{
  int i;
  int j;

  if (ctx->cipher) {
    j = *cipher_index;
    for (i=0; i < len; i++) {
      data[i] ^= ctx->cipher->data[j];
      j = (j+1) % ctx->cipher->len;
    }
    *cipher_index = j;
  }
}


static void Cryptor_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void Cryptor_instance_init(Instance *pi)
{
  Cryptor_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template Cryptor_template = {
  .label = "Cryptor",
  .inputs = Cryptor_inputs,
  .num_inputs = table_size(Cryptor_inputs),
  .outputs = Cryptor_outputs,
  .num_outputs = table_size(Cryptor_outputs),
  .tick = Cryptor_tick,
  .instance_init = Cryptor_instance_init,
};

void Cryptor_init(void)
{
  Template_register(&Cryptor_template);
}
