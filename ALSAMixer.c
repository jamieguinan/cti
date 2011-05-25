/* ALSA mixer controller.   This is limited to a single "simple control".  See
 * output of this command for available controls,
 * 
 *   $ amixer | grep Simple
 *
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <alsa/asoundlib.h>	/* ALSA library */

#include "CTI.h"
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
  String *control_name;
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

  if (!priv->control_name) {
    fprintf(stderr, "no control_name!\n");
    return 1;
  }


  snd_mixer_selem_id_alloca(&sid);

  /* Using the control name, iterate through index values until one
     isn't found.  Typical PC soundcards will only have 1, but M66 has
     4 for DAC and H/W, for example. */
  snd_mixer_selem_id_set_name(sid, priv->control_name->bytes);
  for (i=0; i < 4; i++) {
      snd_mixer_selem_id_set_index(sid, i);
      elem = snd_mixer_find_selem(priv->handle, sid);    

      if (!elem) {
	break;
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

  if (i == 0) {
    /* Warn if no controls matching control_name were found. */
    fprintf(stderr, "unable to find control %s %d\n", 
	    snd_mixer_selem_id_get_name(sid), 
	    snd_mixer_selem_id_get_index(sid));
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

  if (!priv->control_name) {
    fprintf(stderr, "no control_name!\n");    
    return;
  }

  snd_mixer_selem_id_set_name(sid, priv->control_name->bytes);
  snd_mixer_selem_id_set_index(sid, i);  /* get value from the first indexed control */
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

  if (!priv->control_name) {
    fprintf(stderr, "no control_name!\n");    
    return;
  }
  snd_mixer_selem_id_set_name(sid, priv->control_name->bytes);
  snd_mixer_selem_id_set_index(sid, i);  /* get range from the first indexed control */
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


static int set_control_name(Instance *pi, const char *value)
{
  ALSAMixer_private *priv = pi->data;
  priv->control_name = String_new(value);
  return 0;
}

static Config config_table[] = {
  { "card",    set_card, 0L, 0L },
  { "volume",  set_volume, get_volume, get_volume_range },
  { "control_name", set_control_name, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
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
