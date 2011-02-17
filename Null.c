#include <stdio.h>
#include <string.h>

#include "Template.h"
#include "Images.h"
#include "Wav.h"
#include "Mem.h"
#include "Cfg.h"
#include "Log.h"

static void Jpeg_handler(Instance *pi, void *data)
{
  Jpeg_buffer *jpeg_in = data;
  Jpeg_buffer_discard(jpeg_in);
}

static void RGB3_handler(Instance *pi, void *data)
{
  RGB3_buffer *rgb3_in = data;
  RGB3_buffer_discard(rgb3_in);
}

static void Y422P_handler(Instance *pi, void *data)
{
  Y422P_buffer *y422p_in = data;
  Y422P_buffer_discard(y422p_in);
}

static void O511_handler(Instance *pi, void *data)
{
  O511_buffer *o511_in = data;
  O511_buffer_discard(o511_in);
}

static void Wav_handler(Instance *pi, void *data)
{
  Wav_buffer *wav_in = data;
  Wav_buffer_discard(&wav_in);
}

static void RawData_handler(Instance *pi, void *data)
{ 
  RawData_buffer *raw = data;
  RawData_buffer_discard(raw);
}

static Config config_table[] = {
  //  {"...",    set_..., get_..., get_..._range },
};

static void Config_handler(Instance *pi, void *data)
{
  int i;
  Config_buffer *cb_in = data;

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


enum { INPUT_CONFIG, INPUT_JPEG, INPUT_422P, INPUT_RGB3, INPUT_O511, INPUT_WAV, INPUT_RAWDATA };
static Input Null_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_O511 ] = { .type_label = "O511_buffer", .handler = O511_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer",   .handler = Wav_handler },
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
};

// enum { };
static Output Null_outputs[] = { 
};

typedef struct {
} Null_private;


static void Null_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }
}

static void Null_instance_init(Instance *pi)
{
  Null_private *priv = Mem_calloc(1, sizeof(Null_private));
  pi->data = priv;
}

static Template Null_template = {
  .label = "Null",
  .inputs = Null_inputs,
  .num_inputs = table_size(Null_inputs),
  .outputs = Null_outputs,
  .num_outputs = table_size(Null_outputs),
  .tick = Null_tick,
  .instance_init = Null_instance_init,  
};

void Null_init(void)
{
  Template_register(&Null_template);
}

