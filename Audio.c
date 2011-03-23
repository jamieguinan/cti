/* I hope this will be cleaner than using blocks with WAV headers. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */
#include <sys/time.h>		/* gettimeofday */
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
  [CTI_AUDIO_24BIT_SIGNED_LE] = { .size = 3 },
};

Audio_buffer *Audio_buffer_new(int rate, int channels, Audio_type t)
{
  Audio_buffer *buffer = Mem_calloc(1, sizeof(*buffer));
  buffer->type = t;
  buffer->rate = rate;
  buffer->channels = channels;
  buffer->frame_size = channels * TypeMap[t].size;

  return buffer;
}

void Audio_buffer_add_samples(Audio_buffer *buffer, uint8_t *data, int data_len)
{
  int new_length;

  if ((buffer->data_length % buffer->frame_size) != 0) {
    fprintf(stderr, "Audio data_len %" PRIu64 " is not an even number of frames, frame_size=%d\n",
	    buffer->data_length, buffer->frame_size);
    return;
  }
  
  if (!buffer->data) {
    buffer->data = Mem_malloc(data_len);
    new_length = data_len;
  }
  else {
    new_length = buffer->data_length + data_len;
    buffer->data = Mem_realloc(buffer->data, new_length);
  }
  memcpy(buffer->data + buffer->data_length, data, data_len);
  buffer->data_length = new_length;
}

void Audio_buffer_discard(Audio_buffer **buffer)
{
  Audio_buffer *w = *buffer;
  if (w->data) {
    Mem_free(w->data);
  }
  memset(w, 0, sizeof(*w));
  Mem_free(w);
  *buffer = 0L;
}

AAC_buffer *AAC_buffer_from(uint8_t *data, int data_length)
{
  AAC_buffer *buffer = Mem_calloc(1, sizeof(*buffer));

  return buffer;
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
