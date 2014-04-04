#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdint.h>
#include <sys/time.h>
#include "locks.h"

/* 
 * Audio: raw, WAV.  AAC and MP3 are in a separate module since they
 * are optional and may bring in libraries that aren't present
 * on all CTI platforms.
 */
typedef enum {
  CTI_AUDIO_UNKNOWN,
  CTI_AUDIO_8BIT_SIGNED_LE,
  CTI_AUDIO_16BIT_SIGNED_LE,
  CTI_AUDIO_24BIT_SIGNED_LE,
} Audio_type;

/* Raw audio buffers, header is separate from data, and never passed along in data. */

typedef struct {
  uint32_t rate;
  uint32_t frame_size;
  uint16_t channels;
  Audio_type atype;
} Audio_params;

typedef struct {
  Audio_params header;
  uint64_t data_length;
  uint8_t *data;
  double timestamp;
  int seq;
} Audio_buffer;

extern Audio_buffer *Audio_buffer_new(int rate, int channels, Audio_type t);
extern void Audio_buffer_discard(Audio_buffer **buffer);
extern void Audio_buffer_add_samples(Audio_buffer *audio, uint8_t *data, int data_len);


/* 
 * Wav buffer, header is separate from data, but concatenated by MjpegMux.
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
  /* The "params" are somewhat redundant, since they are also in the header,
     but here they are easily accessible and native-endian. */
  Wav_params params;
  int seq;
  int no_feedback;
  int eof;			/* EOF marker. */
  LockedRef ref;
  double timestamp;
} Wav_buffer;

extern int Wav_parse_header_values(unsigned char *src_bytes, 
				   int src_length,
				   uint32_t * rate,
				   uint16_t * channels,
				   uint32_t * frame_size,
				   Audio_type *atype);

extern Wav_buffer *Wav_buffer_new(int rate, int channels, int format_bytes);
extern void Wav_buffer_finalize(Wav_buffer *buffer);
extern void Wav_buffer_release(Wav_buffer **buffer);
extern Wav_buffer *Wav_buffer_from(unsigned char *bytes, int length);
extern Wav_buffer *Wav_ref(Wav_buffer *wav);

/* AAC buffers.  */
typedef struct {
  uint8_t *data;
  int data_length;
  double timestamp;
} AAC_buffer;

extern AAC_buffer *AAC_buffer_from(uint8_t *data, int data_length);
extern void AAC_buffer_discard(AAC_buffer **buffer);


/* MP3 buffers.  */
typedef struct {
  uint8_t *data;
  int data_length;
  double timestamp;
} MP3_buffer;

extern MP3_buffer *MP3_buffer_from(uint8_t *data, int data_length);
extern void MP3_buffer_discard(MP3_buffer **buffer);


#endif
