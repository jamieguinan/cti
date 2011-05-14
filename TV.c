/* TV controller module.  An TV instance lists for IR or other input,
   maintains state, and sends configuration messages to other
   instances that do the actual work.*/
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "TV.h"
#include "Keycodes.h"
#include "ChannelMaps.h"

static void Config_handler(Instance *pi, void *msg);
static void Keycode_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input TV_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_message", .handler = Keycode_handler },
};

enum { OUTPUT_VC_CONFIG, OUTPUT_MIXER_CONFIG };
static Output TV_outputs[] = {
  [ OUTPUT_VC_CONFIG] = { .type_label = "VC_Config_msg", .destination = 0L },
  [ OUTPUT_MIXER_CONFIG] = { .type_label = "Mixer_Config_msg", .destination = 0L },
};

/* States. */
enum { 
  IDLE=0,
  ENTERING_DIGITS,
};

typedef struct {
  int prev_channel;
  int current_channel;
  int new_channel;
  int digits_index;
  const char *map;
} TV_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
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

static void change_channel(Instance *pi)
{
  TV_private *priv = pi->data;
  String *channel_str = String_sprintf("%d",  priv->current_channel);
  const char *freq_str = ChannelMaps_channel_to_frequency(priv->map, channel_str->bytes);

  if (freq_str && pi->outputs[OUTPUT_VC_CONFIG].destination) {
    PostData(Config_buffer_new("frequency", freq_str), 
	     pi->outputs[OUTPUT_VC_CONFIG].destination);
  }

  printf("channel %d\n", priv->current_channel);
  String_free(&channel_str);
}


static void Keycode_handler(Instance *pi, void *msg)
{
  TV_private *priv = pi->data;
  Keycode_message *km = msg;

  // printf("%s: %d\n", __func__, km->keycode);

  switch (km->keycode) {
  case CTI__KEY_0:
  case CTI__KEY_1:
  case CTI__KEY_2:
  case CTI__KEY_3:
  case CTI__KEY_4:
  case CTI__KEY_5:
  case CTI__KEY_6:
  case CTI__KEY_7:
  case CTI__KEY_8:
  case CTI__KEY_9:
    priv->new_channel = (priv->new_channel * 10) + (km->keycode - CTI__KEY_0);
    priv->digits_index += 1;
    if (priv->digits_index == 3) {
      priv->current_channel = priv->new_channel;
      change_channel(pi);
      priv->new_channel = 0;
      priv->digits_index = 0;
    }
    break;

  case CTI__KEY_OK:
    if (priv->digits_index) {
      priv->current_channel = priv->new_channel;
      change_channel(pi);
      priv->new_channel = 0;
      priv->digits_index = 0;
    }
    break;

  case CTI__KEY_CHANNELUP:
    if (priv->current_channel < 78) {
      priv->current_channel += 1;
    }
    goto channelchange;
  case CTI__KEY_CHANNELDOWN: 
    if (priv->current_channel > 2) {
      priv->current_channel -= 1;
    }
  channelchange:
    if (priv->digits_index) {
      priv->new_channel = 0;
      priv->digits_index = 0;
    }
    change_channel(pi);
    break;
  }

  /* Probably do a state machine of sorts here... */
  
}


static void TV_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void TV_instance_init(Instance *pi)
{
  TV_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
  priv->current_channel = 12;
  priv->map = "NTSC_Cable";
}


static Template TV_template = {
  .label = "TV",
  .inputs = TV_inputs,
  .num_inputs = table_size(TV_inputs),
  .outputs = TV_outputs,
  .num_outputs = table_size(TV_outputs),
  .tick = TV_tick,
  .instance_init = TV_instance_init,
};

void TV_init(void)
{
  Template_register(&TV_template);
}
