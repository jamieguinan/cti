#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdint.h>
#include <sys/time.h>

/* 
 * Raw audio.
 */
typedef enum {
  CTI_AUDIO_8BIT_SIGNED_LE,
  CTI_AUDIO_16BIT_SIGNED_LE,
  CTI_AUDIO_24BIT_SIGNED_LE,
} Audio_type;

typedef struct {
  uint64_t data_length;
  uint32_t rate;
  uint32_t frame_size;
  uint16_t type;
  uint16_t channels;
  uint8_t *data;
  struct timeval tv;
  int seq;
} Audio_buffer;

extern Audio_buffer *Audio_buffer_new(int rate, int channels, Audio_type t);
extern void Audio_buffer_discard(Audio_buffer **buffer);

#endif
