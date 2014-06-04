/* I hope this will be cleaner than using blocks with WAV headers. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */
#include <inttypes.h>

#include "Audio.h"
#include "Mem.h"
#include "Log.h"

static struct {
  Audio_type type;
  int size;
} TypeMap[] = {
  [CTI_AUDIO_8BIT_SIGNED_LE] = { .size = 1 },
  [CTI_AUDIO_16BIT_SIGNED_LE] = { .size = 2 },
  [CTI_AUDIO_24BIT_SIGNED_LE] = { .size = 3 }, /* FIXME: Would samples be held in 32-bit buffers? */
};

int Audio_sample_size(Audio_type t)
{
  return TypeMap[t].size;
}


Audio_buffer * Audio_buffer_new(int rate, int channels, Audio_type t)
{
  Audio_buffer *audio = Mem_calloc(1, sizeof(*audio));
  audio->header.atype = t;
  audio->header.rate = rate;
  audio->header.channels = channels;
  audio->header.frame_size = channels * TypeMap[t].size;

  return audio;
}


void Audio_buffer_add_samples(Audio_buffer *audio, uint8_t *data, int data_len)
{
  int new_length;

  if ((audio->data_length % audio->header.frame_size) != 0) {
    fprintf(stderr, "Audio data_len %" PRIu64 " is not an even number of frames, frame_size=%d\n",
	    audio->data_length, audio->header.frame_size);
    return;
  }
  
  if (!audio->data) {
    audio->data = Mem_malloc(data_len);
    new_length = data_len;
  }
  else {
    new_length = audio->data_length + data_len;
    audio->data = Mem_realloc(audio->data, new_length);
  }
  memcpy(audio->data + audio->data_length, data, data_len);
  audio->data_length = new_length;
}


void Audio_buffer_discard(Audio_buffer **audio)
{
  Audio_buffer *w = *audio;
  if (w->data) {
    Mem_free(w->data);
  }
  memset(w, 0, sizeof(*w));
  Mem_free(w);
  *audio = 0L;
}


static uint8_t templateHeader[44] = {
  'R', 'I', 'F', 'F',		/* RIFF file format chunk id */
  0,0,0,0,			/* LE32 file length */
  'W', 'A', 'V', 'E',		/* WAVE chunk id */
  'f', 'm', 't', ' ',		/* format subchunk id (padded) */
  0x10, 0x00, 0x00, 0x00,       /* LE32 length of following (sub)chunk data (16) */
  0x01, 0x00,			/* LE16 Codec ID (WAVE_FORMAT_PCM=1)  */
  0x00, 0x00,			/* LE16 number of channels */
  0,0,0,0,			/* LE32 samples per second */
  0,0,0,0,			/* LE32 bytes per second */
  0,0,				/* LE16 block alignment */
  0,0,				/* LE16 bits per sample */
  'd', 'a', 't', 'a',		/* data subchunk id */
  0,0,0,0,			/* length of following (sub)chunk data */
};

#define FILE_LENGTH_OFFSET 4
#define NUM_CHANNELS_OFFSET 22
#define SAMPLES_PER_SEC_OFFSET 24
#define BYTES_PER_SEC_OFFSET 28
#define BLOCK_ALIGNMENT_OFFSET 32
#define BITS_PER_SAMPLE_OFFSET 34
#define DATA_LENGTH_OFFSET 40
#define DATA_START 44

static void assign_le16(uint8_t *dest, int offset, uint16_t value)
{
  dest[offset+0] = (value >> 0) & 0xff;
  dest[offset+1] = (value >> 8) & 0xff;
}

static uint16_t extract_le16(uint8_t *src, int offset)
{
  uint16_t value;
  value = src[offset+0];
  value |= (src[offset+1] << 8);
  return value;
}


static void assign_le32(uint8_t *dest, int offset, uint32_t value)
{
  dest[offset+0] = (value >> 0) & 0xff;
  dest[offset+1] = (value >> 8) & 0xff;
  dest[offset+2] = (value >> 16) & 0xff;
  dest[offset+3] = (value >> 24) & 0xff;
}

static uint32_t extract_le32(uint8_t *src, int offset)
{
  uint32_t value;
  value = src[offset+0];
  value |= (src[offset+1] << 8);
  value |= (src[offset+2] << 16);
  value |= (src[offset+3] << 24);
  return value;
}


Wav_buffer *Wav_buffer_new(int rate, int channels, int format_bytes)
{
  Wav_buffer *buffer = Mem_calloc(1, sizeof(*buffer));

  memcpy(buffer->header, templateHeader, sizeof(templateHeader));

  assign_le16(buffer->header, NUM_CHANNELS_OFFSET, channels);
  assign_le32(buffer->header, SAMPLES_PER_SEC_OFFSET, rate);
  assign_le32(buffer->header, BYTES_PER_SEC_OFFSET, rate * format_bytes * channels);
  assign_le16(buffer->header, BLOCK_ALIGNMENT_OFFSET, format_bytes * channels);
  assign_le16(buffer->header, BITS_PER_SAMPLE_OFFSET, format_bytes * 8);

  buffer->header_length = 44;

  buffer->params.channels = channels;
  buffer->params.rate = rate;
  buffer->params.bits_per_sample = format_bytes * 8;
  buffer->params.bits_per_sample = format_bytes * 8;
  switch (buffer->params.bits_per_sample) {
  case 8:
    buffer->params.atype = CTI_AUDIO_8BIT_SIGNED_LE;
    break;
  case 16:
    buffer->params.atype = CTI_AUDIO_16BIT_SIGNED_LE;
    break;
  default:
    printf("%s:%s buffer->params.bits_per_sample %d unhandled\n", __FILE__, __func__, buffer->params.bits_per_sample);
  }


  LockedRef_init(&buffer->ref);
  // buffer->data is allocated by caller
  return buffer;
}


void Wav_buffer_finalize(Wav_buffer *buffer)
{
  assign_le32(buffer->header, FILE_LENGTH_OFFSET, buffer->data_length + 44 - 8);
  assign_le32(buffer->header, DATA_LENGTH_OFFSET, buffer->data_length);
}

void Wav_buffer_release(Wav_buffer **buffer)
{
  Wav_buffer *w = *buffer;
  int count;
  LockedRef_decrement(&w->ref, &count);
  if (count == 0) {
    if (w->data) {
      Mem_free(w->data);
    }
    memset(w, 0, sizeof(*w));
    Mem_free(w);
  }
  
  *buffer = 0L;			/* Clear buffer in any case. */
}


int Wav_parse_header_values(unsigned char *src_bytes, 
			    int src_length,
			    uint32_t * rate,
			    uint16_t * channels,
			    uint32_t * frame_size,
			    Audio_type *atype)
{
  if (src_length < 44) {
    fprintf(stderr, "not even enough data for WAV header!\n");
    return 1;
  }

  if (memcmp(src_bytes, templateHeader, 4) != 0) {
    fprintf(stderr, "not a RIFF buffer!\n");
    return 1;
  }

  if (memcmp(src_bytes+8, templateHeader+8, 8) != 0) {
    fprintf(stderr, "expected [WAVEfmt ]!\n");
    return 1;
  }

  *channels = extract_le16(src_bytes, NUM_CHANNELS_OFFSET);
  *rate = extract_le32(src_bytes, SAMPLES_PER_SEC_OFFSET);
  uint16_t bits_per_sample = extract_le16(src_bytes, BITS_PER_SAMPLE_OFFSET);
  *frame_size = (bits_per_sample/8) * (*channels);

  switch (bits_per_sample) {
  case 8: *atype = CTI_AUDIO_8BIT_SIGNED_LE; break;
  case 16: *atype = CTI_AUDIO_16BIT_SIGNED_LE; break;
  case 24: *atype = CTI_AUDIO_24BIT_SIGNED_LE; break;
  }
  
  return 0;
}


Wav_buffer * Wav_buffer_from(unsigned char *src_bytes, int src_length)
{
  /* Extract a wav buffer from the supplied bytes.  Return 0L if invalid in any way. */
  Wav_buffer *buffer;
  uint32_t wav_data_length;

  if (src_length < 44) {
    fprintf(stderr, "not even enough data for WAV header!\n");
    return 0L;
  }

  if (memcmp(src_bytes, templateHeader, 4) != 0) {
    fprintf(stderr, "not a RIFF buffer!\n");
    return 0L;
  }

  if (memcmp(src_bytes+8, templateHeader+8, 8) != 0) {
    fprintf(stderr, "expected [WAVEfmt ]!\n");
    return 0L;
  }

  wav_data_length = extract_le32(src_bytes, DATA_LENGTH_OFFSET);

  if (44+wav_data_length != src_length) {
    fprintf(stderr, "wrong number of source bytes, expected %d got %d)!\n",
	    44+wav_data_length, src_length);
    return 0L;
  }

  /* Good enough, do the allocation now. */
  buffer = Mem_calloc(1, sizeof(*buffer));  

  /* Copy header. */
  memcpy(buffer->header, src_bytes, 44);  
  buffer->header_length = 44;

  /* Copy data. */
  buffer->data = Mem_malloc(wav_data_length);
  memcpy(buffer->data, src_bytes+44, wav_data_length);
  buffer->data_length = wav_data_length;
  
  /* Extract convenience parameters. */
  buffer->params.channels = extract_le16(buffer->header, NUM_CHANNELS_OFFSET);
  buffer->params.rate = extract_le32(buffer->header, SAMPLES_PER_SEC_OFFSET);
  buffer->params.bits_per_sample = extract_le16(buffer->header, BITS_PER_SAMPLE_OFFSET);
  switch (buffer->params.bits_per_sample) {
  case 8:
    buffer->params.atype = CTI_AUDIO_8BIT_SIGNED_LE;
    break;
  case 16:
    buffer->params.atype = CTI_AUDIO_16BIT_SIGNED_LE;
    break;
  default:
    printf("%s:%s buffer->params.bits_per_sample %d unhandled\n", __FILE__, __func__, buffer->params.bits_per_sample);
  }

  LockedRef_init(&buffer->ref);
  return buffer;
}


Wav_buffer * Wav_ref(Wav_buffer * wav)
{
  int count;
  LockedRef_increment(&wav->ref, &count);
  return wav;
}


/* Should this go into AAC.c, or is it Ok here since its not actually
   using the library? */
AAC_buffer *AAC_buffer_from(uint8_t *data, int data_length)
{
  AAC_buffer *aac = Mem_calloc(1, sizeof(*aac));
  aac->data = Mem_malloc(data_length);
  aac->data_length = data_length;
  Mem_memcpy(aac->data, data, data_length);
  return aac;
}

void AAC_buffer_discard(AAC_buffer **buffer)
{
  AAC_buffer *aac = *buffer;
  if (aac->data) {
    Mem_free(aac->data);
  }
  memset(aac, 0, sizeof(*aac));
  Mem_free(aac);
  *buffer = 0L;
}

