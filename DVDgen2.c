/* 
 * Wrapper for external DVD generation tools. 
 * NOTE: Mpeg2enc and Mp2Enc might obviate this!
 */

#warning Mpeg2enc plus Mp2Enc might obviate this module

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "DVDgen2.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422p_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_422P, INPUT_WAV };
static Input DVDgen2_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422p_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_FEEDBACK };
static Output DVDgen2_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L },
};

typedef struct {
  FILE *mpeg2enc;
  FILE *mp2enc;
  Y422P_buffer *y422p_prev;	/* For frame doubling.  Maybe not needed if use current video frame. */
  struct timeval v_time, a_time; /* For tracking time offsets.  Output values, really. */
} DVDgen2_private;

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


static void Y422p_handler(Instance *pi, void *msg)
{
}


static void Wav_handler(Instance *pi, void *msg)
{
}


static void DVDgen2_tick(Instance *pi)
{
  /*
   * Start mpeg2enc and mp2enc with popen(), then feed data. 
   * Track a/v difference, drop video frame if audio is too far behind,
   * double video frame if if audio is too far ahead.  How to finish?
   * Send empty video/audio buffer?
   */

  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void DVDgen2_instance_init(Instance *pi)
{
  DVDgen2_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template DVDgen2_template = {
  .label = "DVDgen2",
  .inputs = DVDgen2_inputs,
  .num_inputs = table_size(DVDgen2_inputs),
  .outputs = DVDgen2_outputs,
  .num_outputs = table_size(DVDgen2_outputs),
  .tick = DVDgen2_tick,
  .instance_init = DVDgen2_instance_init,
};

void DVDgen2_init(void)
{
  Template_register(&DVDgen2_template);
}
