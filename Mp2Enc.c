#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include "CTI.h"
#include "String.h"
#include "Images.h"
#include "Mem.h"
#include "Wav.h"

static void Config_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_WAV };
static Input Mp2Enc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_FEEDBACK };
static Output Mp2Enc_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L  },
};

typedef struct {
  Instance i;
  String *vout;
  FILE *po;			/* File or pipe output... */
  int header_sent;
} Mp2Enc_private;


static int set_vout(Instance *pi, const char *value)
{
  Mp2Enc_private *priv = (Mp2Enc_private *)pi;

  if (priv->vout) {
    String_free(&priv->vout);
  }

  if (priv->po) {
    pclose(priv->po);
  }

  priv->vout = String_new(value);

  char path[256+strlen(priv->vout->bytes)+1];
  // sprintf(path, "cat > %s", priv->vout->bytes);
  fprintf(stderr, "NOTE: mp2enc might for some reason prefer '<' to '|'...\n");
  sprintf(path, "cat | tee test.wav | mp2enc -r 48000 -s -o %s", priv->vout->bytes);
  priv->po = popen(path, "w");

  if (!priv->po) {
    fprintf(stderr, "Mp2Enc:set_vout failed!\n");
  }

  return 0;
}


static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
  { "aout", set_vout, 0L, 0L},
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Wav_handler(Instance *pi, void *msg)
{
  Mp2Enc_private *priv = (Mp2Enc_private *)pi;
  Wav_buffer *wav_in = msg;
  int n;

  if (!priv->po) {
    fprintf(stderr, "%s: pipe output not set!\n", __func__);
    goto out;
  }
    
  if (!priv->header_sent) {
    /* Create and write header. */
    n = fwrite(wav_in->header, 44, 1, priv->po);
    if (n != 1) {
      perror(priv->vout->bytes);
      pclose(priv->po);    
      priv->po = 0L;
      goto out;
    }

    priv->header_sent = 1;
  }

  /* Create and write one block. */
  n = fwrite(wav_in->data, wav_in->data_length, 1, priv->po);

  if (n != 1) {
    perror(priv->vout->bytes);
    pclose(priv->po);    
    priv->po = 0L;
  }
  
 out:
  Wav_buffer_discard(&wav_in);
}


static void Mp2Enc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void Mp2Enc_instance_init(Instance *pi)
{
  // Mp2Enc_private *priv = (Mp2Enc_private *)pi;
}


static Template Mp2Enc_template = {
  .label = "Mp2Enc",
  .priv_size = sizeof(Mp2Enc_private),
  .inputs = Mp2Enc_inputs,
  .num_inputs = table_size(Mp2Enc_inputs),
  .outputs = Mp2Enc_outputs,
  .num_outputs = table_size(Mp2Enc_outputs),
  .tick = Mp2Enc_tick,
  .instance_init = Mp2Enc_instance_init,
};

void Mp2Enc_init(void)
{
  Template_register(&Mp2Enc_template);
}
