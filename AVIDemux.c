/* Maybe rename this "RIFFDemux"? */
#include <stdio.h>
#include <string.h>             /* memcpy */
#include <stdlib.h>             /* free */

#include "CTI.h"
#include "Mem.h"
#include "AVIDemux.h"
#include "SourceSink.h"
#include "Wav.h"
#include "Images.h"

  /* Quick macro to align file offets to even bytes. */
#define even(x) ((x) % 2 ? ((x)+1):(x))

enum { CHUNK_UNKNOWN, CHUNK_AUDIO, CHUNK_JPEG, CHUNK_IDX };

static void Config_handler(Instance *pi, void *msg);
static void Feedback_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_FEEDBACK, INPUT_KEYCODE };
static Input AVIDemux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_JPEG, OUTPUT_WAV };
static Output AVIDemux_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  Source *source;
  ArrayU8 *chunk;
  int needData;
  int done;
  int enable;                   /* Set this to start processing. */
  int synchronized;

  int video_frame_period_ms;
  int audio_channels;
  int audio_hz;
} AVIDemux_private;


static ArrayU8 * audio_marker = ArrayU8_temp_const("01wb", 4);
static ArrayU8 * jpeg_marker = ArrayU8_temp_const("00dc", 4);
static ArrayU8 * idx_marker = ArrayU8_temp_const("idx1", 4);


static int set_input(Instance *pi, const char *value)
{
  AVIDemux_private *priv = (AVIDemux_private *)pi;
  if (priv->source) {
    Source_free(&priv->source);
  }
  priv->source = Source_new(value);

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  AVIDemux_private *priv = (AVIDemux_private *)pi;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "AVIDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }

  printf("AVIDemux enable set to %d\n", priv->enable);

  return 0;
}


static Config config_table[] = {
  { "input",    set_input, 0L, 0L },
  { "enable",    set_enable, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Keycode_handler(Instance *pi, void *msg)
{
}


static void Feedback_handler(Instance *pi, void *msg)
{
}


static int avi_chunk_ready(ArrayU8 *chunk, const ArrayU8 *marker, int *length)
{
  if (chunk->len < 8) {
    return 0;
  }

  if (memcmp(chunk->data, marker->data, 4) != 0) {
    return 0;
  }

  uint32_t avi_chunk_size;
  uint32_t required_data;
  ArrayU8_extract_uint32le(chunk, 4, &avi_chunk_size);
  required_data = even(4 + 4 + avi_chunk_size);
  if (chunk->len < required_data) {
    return 0;
  }

  *length = avi_chunk_size;
  return 1;
}


static void AVIDemux_tick(Instance *pi)
{
  Handler_message *hm;
  AVIDemux_private *priv = (AVIDemux_private *)pi;
  int wait_flag;

  if (!priv->enable || !priv->source
      /* || (priv->pending_feedback >= priv->feedback_threshold)  */
      ) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable || !priv->source) {
    return;
  }

  if (priv->needData) {
    Source_acquire_data(priv->source, priv->chunk, &priv->needData);
  }

  /*
   *  Should be looking for:
   *     "01wb"  [audio data]
   *     "00dc"  [ffd8 Jpeg marker, jpeg data, including "AVI1" tag]
   *     "idx1"  [ data chunk indexes before EOF ]

   80c: "01wb"
   810: 10 2b 00 00
   814: 0x00002b10 bytes of data...
   3324: 00dc
   */
  if (Source_tell(priv->source) == 0 && !priv->synchronized) {
    /* Parse AVI header. */

  }

  if (!priv->synchronized) {
    /* Search for nearest instance of any valid marker. */
    int a_index, j_index, i_index;
    int index = -1;
    a_index = ArrayU8_search(priv->chunk, 0, audio_marker);
    if (a_index >= 0) {
      index = a_index;
    }

    j_index = ArrayU8_search(priv->chunk, 0, jpeg_marker);
    if (j_index >= 0) {
      if (index == -1) {
        index = j_index;
      }
      else if (j_index < index) {
        index = j_index;
      }
    }

    i_index = ArrayU8_search(priv->chunk, 0, idx_marker);
    if (i_index >= 0) {
      if (index == -1) {
        index = i_index;
      }
      else if (i_index < index) {
        index = i_index;
      }
    }

    if (index >= 0) {
      ArrayU8_trim_left(priv->chunk, index);
      priv->synchronized = 1;
    }

    if (!priv->synchronized) {
      /* Still not synchronized! */
      priv->needData = 1;
      return;
    }
  }

  int avi_chunk_size;
  if (avi_chunk_ready(priv->chunk, audio_marker, &avi_chunk_size)) {
    if (pi->outputs[OUTPUT_WAV].destination) {
      Wav_buffer *wav =  Wav_buffer_from(priv->chunk->data+8, avi_chunk_size);
      PostData(wav, pi->outputs[OUTPUT_WAV].destination);
    }
    ArrayU8_trim_left(priv->chunk, even(8+avi_chunk_size));
  }
  else if (avi_chunk_ready(priv->chunk, jpeg_marker, &avi_chunk_size)) {
    if (pi->outputs[OUTPUT_JPEG].destination) {
      Jpeg_buffer *jpeg = Jpeg_buffer_from(priv->chunk->data+8, avi_chunk_size, 0L);
      PostData(jpeg, pi->outputs[OUTPUT_JPEG].destination);
    }
    ArrayU8_trim_left(priv->chunk, even(8+avi_chunk_size));
  }
  else if (avi_chunk_ready(priv->chunk, idx_marker, &avi_chunk_size)) {
    ArrayU8_trim_left(priv->chunk, even(8+avi_chunk_size));
  }
  else {
    priv->needData = 1;
  }

  if (priv->chunk->len < 32000) {
    /* Maybe avoid an extra tick pass. */
    priv->needData = 1;
  }
}


static void AVIDemux_instance_init(Instance *pi)
{
  // AVIDemux_private *priv = (AVIDemux_private *)pi;
}


static Template AVIDemux_template = {
  .label = "AVIDemux",
  .priv_size = sizeof(AVIDemux_private),
  .inputs = AVIDemux_inputs,
  .num_inputs = table_size(AVIDemux_inputs),
  .outputs = AVIDemux_outputs,
  .num_outputs = table_size(AVIDemux_outputs),
  .tick = AVIDemux_tick,
  .instance_init = AVIDemux_instance_init,
};


void AVIDemux_init(void)
{
  Template_register(&AVIDemux_template);
}

