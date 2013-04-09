#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>
#include <sys/time.h>

/* 
 * Wav buffer.
 * References:
 *   https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
 *   http://tools.ietf.org/html/rfc2361
 */

typedef struct {
  int codec_id;
  uint16_t channels;
  uint32_t rate;
  uint16_t bits_per_sample;
} Wav_params;

typedef struct {
  uint8_t header[44];
  int header_length;
  void *data;
  int data_length;
  /* timestamp? */
  /* The "params" are redundant, since they are also in the header,
     but here they are easily accessible and native-endian. */
  Wav_params params;
  struct timeval tv;
  int seq;
  int no_feedback;
  int eof;			/* EOF marker. */
} Wav_buffer;

extern Wav_buffer *Wav_buffer_new(int rate, int channels, int format_bytes);
extern void Wav_buffer_finalize(Wav_buffer *buffer);
extern void Wav_buffer_discard(Wav_buffer **buffer);

extern Wav_buffer *Wav_buffer_from(unsigned char *bytes, int length);

#endif
