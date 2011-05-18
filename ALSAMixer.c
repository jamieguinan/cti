/* ALSA mixer controller. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <alsa/asoundlib.h>	/* ALSA library */

#include "Template.h"
#include "ALSAMixer.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ALSAMixer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output ALSAMixer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  int card_index;
  char card[64];
  snd_mixer_t *handle;
  int volume;
  // int max_volume;
  String *control_list;
} ALSAMixer_private;

static int set_card(Instance *pi, const char *value)
{
  ALSAMixer_private *priv = pi->data;
  int rc;

  priv->card_index = snd_card_get_index(value);
  printf("card index %d\n", priv->card_index);
  sprintf(priv->card, "hw:%d", priv->card_index);

  rc = snd_mixer_open(&priv->handle, 0);
  if (rc < 0) {
    perror("snd_mixer_open"); goto out;
  }

  rc = snd_mixer_attach(priv->handle, priv->card);
  if (rc < 0) {
    perror("snd_mixer_attach"); goto out;
  }

  rc = snd_mixer_selem_register(priv->handle, NULL, NULL);
  if (rc < 0) {
    perror("snd_mixer_selem_register"); goto out;
  }

  rc = snd_mixer_load(priv->handle);
  if (rc < 0) {
    perror("snd_mixer_load"); goto out;
  }

 out:
  return rc;
}


static int set_volume(Instance *pi, const char *value)
{
  ALSAMixer_private *priv = pi->data;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  long vmin, vmax, vol;
  int i;
  int temp = atoi(value);

  if (!priv->handle) {
    fprintf(stderr, "no handle yet!\n");
    return 1;
  }

  snd_mixer_selem_id_alloca(&sid);

  /* FIXME: For M66, I know I want to get/set DAC,0 and DAC,1 (and
     mabye 3 and 4 for the subwoofer).  To make this more generic,
     maybe make a list as a priv member, and iterate through it. */
  for (i=0; i < 4; i++) {
      snd_mixer_selem_id_set_name(sid, "DAC");
      snd_mixer_selem_id_set_index(sid, i);
      elem = snd_mixer_find_selem(priv->handle, sid);    

      if (!elem) {
	fprintf(stderr, "unable to find control %s %d\n", 
		snd_mixer_selem_id_get_name(sid), 
		snd_mixer_selem_id_get_index(sid));
	return 1;
      }

      if (!snd_mixer_selem_has_playback_volume(elem)) {
	fprintf(stderr, "playback volume unavailable!\n");
      }

      snd_mixer_selem_get_playback_volume_range(elem, &vmin, &vmax);
      snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &vol);

      if (vmin <= temp && temp < vmax) {
	priv->volume = temp;    
	snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_MONO, temp);
	printf("volume: %ld -> %d\n", vol, temp);
      }

  }

  return 0;
}

static void get_volume(Instance *pi, Value *value)
{
  printf("%s: %s\n", __FILE__, __func__);
  ALSAMixer_private *priv = pi->data;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  long vol;
  int i=0;

  if (!priv->handle) {
    fprintf(stderr, "no handle yet!\n");
    return;
  }


  snd_mixer_selem_id_alloca(&sid);

  snd_mixer_selem_id_set_name(sid, "DAC");
  snd_mixer_selem_id_set_index(sid, i);
  elem = snd_mixer_find_selem(priv->handle, sid);    

  if (!elem) {
    fprintf(stderr, "unable to find control %s %d\n", 
	    snd_mixer_selem_id_get_name(sid), 
	    snd_mixer_selem_id_get_index(sid));
    return;
  }
  
  if (!snd_mixer_selem_has_playback_volume(elem)) {
    fprintf(stderr, "playback volume unavailable!\n");
  }
  
  snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &vol);
  value->type = RANGE_INTS;
  value->u.int_value = vol;
}


static void get_volume_range(Instance *pi, Range *range)
{
  printf("%s: %s\n", __FILE__, __func__);
  ALSAMixer_private *priv = pi->data;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  long vmin, vmax;
  int i=0;

  if (!priv->handle) {
    fprintf(stderr, "no handle yet!\n");
    return;
  }

  snd_mixer_selem_id_alloca(&sid);

  snd_mixer_selem_id_set_name(sid, "DAC");
  snd_mixer_selem_id_set_index(sid, i);
  elem = snd_mixer_find_selem(priv->handle, sid);    

  if (!elem) {
    fprintf(stderr, "unable to find control %s %d\n", 
	    snd_mixer_selem_id_get_name(sid), 
	    snd_mixer_selem_id_get_index(sid));
    return;
  }
  
  if (!snd_mixer_selem_has_playback_volume(elem)) {
    fprintf(stderr, "playback volume unavailable!\n");
  }
  
  snd_mixer_selem_get_playback_volume_range(elem, &vmin, &vmax);
  range->type = RANGE_INTS;
  range->u.ints.min = vmin;
  range->u.ints.max = vmax;
  range->u.ints.step = 1;
  range->u.ints._default = 0;
}


static Config config_table[] = {
  { "card",    set_card, 0L, 0L },
  { "volume",  set_volume, get_volume, get_volume_range },
  { "control_list", set_control_list, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

  printf("%s:%s  label=%p value=%p vreq=%p\n", __FILE__, __func__, cb_in->label, cb_in->value, cb_in->vreq);

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {

      /* If value is passed in, call the set function. */
      if (cb_in->value) {
	int rc;		/* FIXME: What to do with this? */
	rc = config_table[i].set(pi, cb_in->value->bytes);
      }

      /* Check and full range/value requests. */
      if (cb_in->vreq && config_table[i].get_value) {
	config_table[i].get_value(pi, cb_in->vreq);
      }

      if (cb_in->rreq && config_table[i].get_range) {
	config_table[i].get_range(pi, cb_in->rreq);
      }

      /* Wake caller if they asked for it. */
      if (cb_in->wake) {
	Event_signal(cb_in->wake);
      }
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
}


static void ALSAMixer_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void ALSAMixer_instance_init(Instance *pi)
{
  ALSAMixer_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
  strcpy(priv->card, "default");
}


static Template ALSAMixer_template = {
  .label = "ALSAMixer",
  .inputs = ALSAMixer_inputs,
  .num_inputs = table_size(ALSAMixer_inputs),
  .outputs = ALSAMixer_outputs,
  .num_outputs = table_size(ALSAMixer_outputs),
  //.configs = config_table,
  //.num_configs = table_size(config_table),
  .tick = ALSAMixer_tick,
  .instance_init = ALSAMixer_instance_init,
};

void ALSAMixer_init(void)
{
  Template_register(&ALSAMixer_template);
}
