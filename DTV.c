/* ClearQAM/mplayer controller module.  Listens for IR or other input,
   maintains state, runs mplayer dvb://channel */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* fork */
#include <sys/types.h>		/* kill, waitpid */
#include <signal.h>		/* kill */
#include <sys/wait.h>		/* waitpid */

#include "CTI.h"
#include "DTV.h"
#include "CSV.h"
#include "String.h"
#include "Keycodes.h"

static void Config_handler(Instance *pi, void *msg);
static void Keycode_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input DTV_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_message", .handler = Keycode_handler },
};

enum { OUTPUT_MIXER_CONFIG, OUTPUT_CAIRO_CONFIG };
static Output DTV_outputs[] = {
  [ OUTPUT_MIXER_CONFIG] = { .type_label = "Mixer_Config_msg", .destination = 0L },
  [ OUTPUT_CAIRO_CONFIG] = { .type_label = "Cairo_Config_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  int power_state;
  int num_channels;
  int current_channel_index;
  int mute_save_vol;
  int muted;
  CSV_table *channels_csv;
  pid_t child_pid;
} DTV_private;

static Config config_table[] = {
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void kill_subproc(Instance *pi)
{
  /* Kill subprocess, if one is running. */
  DTV_private *priv = (DTV_private *)pi;

  if (priv->child_pid != -1) {
    int status;
    kill(priv->child_pid, SIGQUIT);
    pid_t pid =  waitpid(priv->child_pid, &status, 0);
    if (pid > 0 && pid == priv->child_pid) {
      priv->child_pid = -1;
    }
  }
}

static void change_channel(Instance *pi)
{
  DTV_private *priv = (DTV_private *)pi;
  String *channel_str = CSV_get(priv->channels_csv, priv->current_channel_index, 0);
  String *dvb_channel_str = String_sprintf("dvb://%s", channel_str->bytes);

  printf("channel %d (%s)\n", priv->current_channel_index, dvb_channel_str->bytes);

  kill_subproc(pi);

  priv->child_pid = fork();
  if (priv->child_pid == 0) {
    /* Child. */
    /* FIXME: This only allows for a single command with no parameters. */
    execlp("xterm", "xterm", "-e", "mplayer", "-fs", dvb_channel_str->bytes, NULL);
    perror("execl");
    exit(1);
  }
}


static void Keycode_handler(Instance *pi, void *msg)
{
  DTV_private *priv = (DTV_private *)pi;
  Keycode_message *km = msg;
  int d = 0;
  int mute = 0;
  // printf("%s: %d\n", __func__, km->keycode);

  switch (km->keycode) {
  case CTI__KEY_CHANNELUP:
    if (priv->current_channel_index < priv->num_channels) {
      priv->current_channel_index += 1;
    }
    goto channelchange;
  case CTI__KEY_CHANNELDOWN: 
    if (priv->current_channel_index > 0) {
      priv->current_channel_index -= 1;
    }
  channelchange:
    if (priv->power_state) {
      change_channel(pi);
    }
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

      if (pi->outputs[OUTPUT_CAIRO_CONFIG].destination) {
	if (priv->muted) {
	  snprintf(temp, sizeof(temp), "MUTE");
	}
	else {
	  snprintf(temp, sizeof(temp), "VOL %d%%", (vol*100/range.ints.max));
	}
	PostData(Config_buffer_new("text", temp), pi->outputs[OUTPUT_CAIRO_CONFIG].destination);
      }
    }
    break;

  case CTI__KEY_POWER:
    /* FIXME:  Could just track state, and turn off the running subproc... */
    if (priv->power_state) {
      priv->power_state = 0;
      kill_subproc(pi);
    }
    else {
      priv->power_state = 1;
      change_channel(pi);
    }
    break;
  }

  Keycode_message_cleanup(&km);
}


static void DTV_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void DTV_instance_init(Instance *pi)
{
  DTV_private *priv = (DTV_private *)pi;
  
  priv->channels_csv = CSV_load(String_sprintf("%s/projects/av/channels.csv.000", getenv("HOME")));
  printf("%d rows %d columns\n", priv->channels_csv->_rows, priv->channels_csv->_columns);
  priv->current_channel_index = 0;
  priv->child_pid = -1;
  if (0) {
    priv->power_state = 0;
  }
  else {
    priv->power_state = 1;
    change_channel(pi);
  }
}


static Template DTV_template = {
  .label = "DTV",
  .priv_size = sizeof(DTV_private),
  .inputs = DTV_inputs,
  .num_inputs = table_size(DTV_inputs),
  .outputs = DTV_outputs,
  .num_outputs = table_size(DTV_outputs),
  .tick = DTV_tick,
  .instance_init = DTV_instance_init,
};

void DTV_init(void)
{
  Template_register(&DTV_template);
}
