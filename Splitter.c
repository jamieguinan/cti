/* 
 * Duplicates or references inputs, distributes to outputs.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Splitter.h"
#include "Images.h"
#include "Wav.h"

static void Config_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *data);
static void Jpeg_handler(Instance *pi, void *data);

#define NUM_OUTPUTS 2

enum { INPUT_CONFIG, INPUT_YUV422P, INPUT_JPEG, INPUT_WAV };
static Input Splitter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
};

enum { OUTPUT_YUV422P_1, OUTPUT_YUV422P_2, OUTPUT_JPEG_1, OUTPUT_JPEG_2,
       OUTPUT_WAV_1, OUTPUT_WAV_2 };
static Output Splitter_outputs[] = {
  [ OUTPUT_YUV422P_1 ] = {.type_label = "YUV422P_buffer.1", .destination = 0L },
  [ OUTPUT_YUV422P_2 ] = {.type_label = "YUV422P_buffer.2", .destination = 0L },
  [ OUTPUT_JPEG_1 ] = {.type_label = "Jpeg_buffer.1", .destination = 0L },
  [ OUTPUT_JPEG_2 ] = {.type_label = "Jpeg_buffer.2", .destination = 0L },
  [ OUTPUT_WAV_1 ] = {.type_label = "Wav_buffer.1", .destination = 0L },
  [ OUTPUT_WAV_2 ] = {.type_label = "Wav_buffer.2", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} Splitter_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void y422p_handler(Instance *pi, void *data)
{
  const int outputs[NUM_OUTPUTS] = { OUTPUT_YUV422P_1, OUTPUT_YUV422P_2 };
  int i, j;
  int out_count = 0;
  YUV422P_buffer *y422p_in = data;

  for (i=0; i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count += 1;
    }
  }

  for (i=0; out_count && i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count -= 1;
      if (out_count) {
	/* Clone and post buffer. */
	YUV422P_buffer *y422p_tmp = YUV422P_clone(y422p_in);
	PostData(y422p_tmp, pi->outputs[j].destination); 
	y422p_tmp = NULL;
      }
      else {
	/* Post buffer. */
	PostData(y422p_in, pi->outputs[j].destination); 
	y422p_in = 0L;
      }
    }
  }

  if (y422p_in) {
    /* There were no outputs. */
    YUV422P_buffer_discard(y422p_in);
  }
}


static void Jpeg_handler(Instance *pi, void *data)
{
  const int outputs[NUM_OUTPUTS] = { OUTPUT_JPEG_1, OUTPUT_JPEG_2 };
  int i, j;
  int out_count = 0;
  Jpeg_buffer *jpeg_in = data;

  for (i=0; i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count += 1;
    }
  }

  for (i=0; out_count && i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count -= 1;
      if (out_count) {
	/* Clone and post buffer. */
	Jpeg_buffer *Jpeg_tmp = Jpeg_buffer_ref(jpeg_in);
	PostData(Jpeg_tmp, pi->outputs[j].destination); 
	Jpeg_tmp = NULL;
      }
      else {
	/* Post buffer. */
	PostData(jpeg_in, pi->outputs[j].destination); 
	jpeg_in = 0L;
      }
    }
  }

  if (jpeg_in) {
    /* There were no outputs. */
    Jpeg_buffer_discard(jpeg_in);
  }
}


static void Wav_handler(Instance *pi, void *data)
{
  const int outputs[NUM_OUTPUTS] = { OUTPUT_WAV_1, OUTPUT_WAV_2 };
  int i, j;
  int out_count = 0;
  Wav_buffer *wav_in = data;

  for (i=0; i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count += 1;
    }
  }

  for (i=0; out_count && i < NUM_OUTPUTS; i++) {
    j = outputs[i];
    if (pi->outputs[j].destination) {
      out_count -= 1;
      if (out_count) {
	/* Clone and post buffer. */
	Wav_buffer *Wav_tmp = Wav_ref(wav_in);
	PostData(Wav_tmp, pi->outputs[j].destination); 
	Wav_tmp = NULL;
      }
      else {
	/* Post buffer. */
	PostData(wav_in, pi->outputs[j].destination); 
	wav_in = 0L;
      }
    }
  }

  if (wav_in) {
    /* There were no outputs. */
    Wav_buffer_release(&wav_in);
  }
}


static void Splitter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Splitter_instance_init(Instance *pi)
{
  // Splitter_private *priv = (Splitter_private *)pi;
}


static Template Splitter_template = {
  .label = "Splitter",
  .priv_size = sizeof(Splitter_private),
  .inputs = Splitter_inputs,
  .num_inputs = table_size(Splitter_inputs),
  .outputs = Splitter_outputs,
  .num_outputs = table_size(Splitter_outputs),
  .tick = Splitter_tick,
  .instance_init = Splitter_instance_init,
};

void Splitter_init(void)
{
  Template_register(&Splitter_template);
}
