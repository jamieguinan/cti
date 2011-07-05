#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */
#include <sys/time.h>		/* gettimeofday */

#include "Wav.h"
#include "Mem.h"
#include "Log.h"

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

  // buffer->params.codec_id = ??;
  buffer->params.channels = channels;
  buffer->params.rate = rate;
  buffer->params.bits_per_sample = format_bytes * 8;
  
  // buffer->data is allocated by caller
  return buffer;
}


void Wav_buffer_finalize(Wav_buffer *buffer)
{
  assign_le32(buffer->header, FILE_LENGTH_OFFSET, buffer->data_length + 44 - 8);
  assign_le32(buffer->header, DATA_LENGTH_OFFSET, buffer->data_length);
}

void Wav_buffer_discard(Wav_buffer **buffer)
{
  Wav_buffer *w = *buffer;
  if (w->data) {
    Mem_free(w->data);
  }
  memset(w, 0, sizeof(*w));
  Mem_free(w);
  *buffer = 0L;
}

Wav_buffer *Wav_buffer_from(unsigned char *src_bytes, int src_length)
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

  return buffer;
}

