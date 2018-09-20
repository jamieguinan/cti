/* TV controller module.  An TV instance listens for IR or other input,
   maintains state, and sends configuration messages to other
   instances that do the actual work.*/
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"
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

enum { OUTPUT_VC_CONFIG, OUTPUT_MIXER_CONFIG, OUTPUT_CAIRO_CONFIG };
static Output TV_outputs[] = {
  [ OUTPUT_VC_CONFIG] = { .type_label = "VC_Config_msg", .destination = 0L },
  [ OUTPUT_MIXER_CONFIG] = { .type_label = "Mixer_Config_msg", .destination = 0L },
  [ OUTPUT_CAIRO_CONFIG] = { .type_label = "Cairo_Config_msg", .destination = 0L },
};

/* States. */
enum {
  IDLE=0,
  ENTERING_DIGITS,
};

typedef struct {
  Instance i;
  int prev_channel;
  int current_channel;
  int new_channel;
  int digits_index;
  int mute_save_vol;
  int muted;
  const char *map;
  uint32_t skip_channels[1000/32]; /* 1000 channels, represented by one bit per channel */
} TV_private;


static int set_skip_channel(Instance *pi, const char *value)
{
  TV_private *priv = (TV_private *)pi;
  int channel = atoi(value);
  if (0 < channel && channel < 1000) {
    priv->skip_channels[channel/32] |= (1 << channel % 32);
    return 0;
  }
  else {
    fprintf(stderr, "channel %d out of range\n", channel);
    return 1;
  }
}


static int _get_skip_channel(TV_private *priv, int channel)
{

  if (priv->skip_channels[channel/32] & (1 << (channel % 32))) {
    return 1;
  }
  else {
    return 0;
  }
}


static Config config_table[] = {
  { "skip_channel",  set_skip_channel, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void change_channel(Instance *pi)
{
  TV_private *priv = (TV_private *)pi;
  String *channel_str = String_sprintf("%d",  priv->current_channel);
  const char *freq_str = ChannelMaps_channel_to_frequency(priv->map, channel_str->bytes);

  if (freq_str && pi->outputs[OUTPUT_VC_CONFIG].destination) {
    PostData(Config_buffer_new("frequency", freq_str),
             pi->outputs[OUTPUT_VC_CONFIG].destination);
  }

  printf("channel %d\n", priv->current_channel);
  String_free(&channel_str);

  if (pi->outputs[OUTPUT_CAIRO_CONFIG].destination) {
    char temp[64];
    snprintf(temp, sizeof(temp), "%d", priv->current_channel);
    PostData(Config_buffer_new("text", temp), pi->outputs[OUTPUT_CAIRO_CONFIG].destination);
  }

}


static void Keycode_handler(Instance *pi, void *msg)
{
  TV_private *priv = (TV_private *)pi;
  Keycode_message *km = msg;
  int d = 0;
  int mute = 0;
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
    else {
      if (pi->outputs[OUTPUT_CAIRO_CONFIG].destination) {
        char temp[64];
        snprintf(temp, sizeof(temp), "%d", priv->new_channel);
        PostData(Config_buffer_new("text", temp), pi->outputs[OUTPUT_CAIRO_CONFIG].destination);
      }
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
    while (priv->current_channel < 78) {
      priv->current_channel += 1;
      if (_get_skip_channel(priv, priv->current_channel) == 0) {
        break;
      }
    }
    goto channelchange;
  case CTI__KEY_CHANNELDOWN:
    while (priv->current_channel > 2) {
      priv->current_channel -= 1;
      if (_get_skip_channel(priv, priv->current_channel) == 0) {
        break;
      }
    }
  channelchange:
    if (priv->digits_index) {
      priv->new_channel = 0;
      priv->digits_index = 0;
    }
    change_channel(pi);
    break;

  case CTI__KEY_VOLUMEDOWN:
    d = -5;
    goto next;
  case CTI__KEY_VOLUMEUP:
    d = +5;
    goto next;
  case CTI__KEY_MUTE:
    mute = 1;
  next:
    if (pi->outputs[OUTPUT_MIXER_CONFIG].destination) {
      Value value = {};
      Range range = {};
      char temp[64];
      int vol;
      GetConfigRange(pi->outputs[OUTPUT_MIXER_CONFIG].destination, "volume", &range);
      GetConfigValue(pi->outputs[OUTPUT_MIXER_CONFIG].destination, "volume", &value);
      vol = value.u.int_value  + d;
      if (mute) {
        if (!priv->muted) {
          priv->mute_save_vol = vol;
          vol = 0;
          priv->muted = 1;
        }
        else {
          vol = priv->mute_save_vol;
          priv->muted = 0;
        }
      }
      else {
        /* Volume up/down selected. */
        if (priv->muted) {
          vol = priv->mute_save_vol;
          priv->muted = 0;
        }
      }
      if (vol < range.ints.min) vol = range.ints.min;
      else if (vol > range.ints.max) vol = range.ints.max;
      sprintf(temp, "%d", vol);
      PostData(Config_buffer_new("volume", temp), pi->outputs[OUTPUT_MIXER_CONFIG].destination);

      if (priv->muted) {
        snprintf(temp, sizeof(temp), "MUTE");
      }
      else {
        snprintf(temp, sizeof(temp), "VOL %d%%", (vol*100/range.ints.max));
      }
      PostData(Config_buffer_new("text", temp), pi->outputs[OUTPUT_CAIRO_CONFIG].destination);
    }
    break;

  case CTI__KEY_POWER:
    exit(0);
    break;
  }

  /* Probably do a state machine of sorts here... if its needed. */

  Keycode_message_cleanup(&km);
}


static void TV_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void TV_instance_init(Instance *pi)
{
  TV_private *priv = (TV_private *)pi;

  priv->current_channel = 12;
  priv->map = "NTSC_Cable";
}


static Template TV_template = {
  .label = "TV",
  .priv_size = sizeof(TV_private),
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
