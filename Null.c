#include <stdio.h>
#include <string.h>

#include "CTI.h"
#include "Images.h"
#include "Wav.h"
#include "Audio.h"
#include "Mem.h"
#include "Cfg.h"
#include "Log.h"

static void Jpeg_handler(Instance *pi, void *data)
{
  Jpeg_buffer *jpeg_in = data;
  Jpeg_buffer_release(jpeg_in);
}


static void Gray_handler(Instance *pi, void *data)
{
  Gray_buffer *jpeg_in = data;
  Gray_buffer_release(jpeg_in);
}


static void RGB3_handler(Instance *pi, void *data)
{
  RGB3_buffer *rgb3_in = data;
  RGB3_buffer_release(rgb3_in);
}

static void YUV422P_handler(Instance *pi, void *data)
{
  YUV422P_buffer *y422p_in = data;
  YUV422P_buffer_release(y422p_in);
}

static void YUV420P_handler(Instance *pi, void *data)
{
  YUV420P_buffer *y420p_in = data;
  YUV420P_buffer_release(y420p_in);
}

static void O511_handler(Instance *pi, void *data)
{
  O511_buffer *o511_in = data;
  O511_buffer_release(o511_in);
}

static void Wav_handler(Instance *pi, void *data)
{
  Wav_buffer *wav_in = data;
  Wav_buffer_release(&wav_in);
}

static void Audio_handler(Instance *pi, void *data)
{
  Audio_buffer *audio_in = data;
  Audio_buffer_release(&audio_in);
}

static void H264_handler(Instance *pi, void *data)
{
  H264_buffer *h264 = data;
  H264_buffer_release(h264);
}

static void AAC_handler(Instance *pi, void *data)
{
  AAC_buffer *aac = data;
  AAC_buffer_release(&aac);
}

static void RawData_handler(Instance *pi, void *data)
{ 
  RawData_buffer *raw = data;
  RawData_buffer_release(raw);
}

static Config config_table[] = {
  //  {"...",    set_..., get_..., get_..._range },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


enum { INPUT_CONFIG, INPUT_GRAY, INPUT_JPEG, INPUT_YUV422P, INPUT_YUV420P, INPUT_RGB3, INPUT_O511, INPUT_WAV, INPUT_AUDIO, INPUT_H264, INPUT_AAC, INPUT_RAWDATA };
static Input Null_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY ] = { .type_label = "GRAY_buffer", .handler = Gray_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = YUV422P_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_O511 ] = { .type_label = "O511_buffer", .handler = O511_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer",   .handler = Wav_handler },
  [ INPUT_AUDIO ] = { .type_label = "Audio_buffer",   .handler = Audio_handler },
  [ INPUT_H264 ] = { .type_label = "H264_buffer", .handler = H264_handler },
  [ INPUT_AAC ] = { .type_label = "AAC_buffer",   .handler = AAC_handler },
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
};

// enum { };
static Output Null_outputs[] = { 
};

typedef struct {
  Instance i;
} Null_private;


static void Null_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    // printf("%s handler called, releaseing message...\n", __func__);
    ReleaseMessage(&hm,pi);
  }
}

static void Null_instance_init(Instance *pi)
{
}

static Template Null_template = {
  .label = "Null",
  .priv_size = sizeof(Null_private),
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

