#ifndef _ALSAIO_H_
#define _ALSAIO_H_

#include <alsa/asoundlib.h>	/* ALSA library */
#include <stdint.h>
#include "Audio.h"		/*  Audio_type */

extern void ALSAio_init(void);

typedef enum { ALSAIO_READ=1, ALSAIO_WRITE } ALSAio_rw_enum;

typedef struct {
  snd_pcm_t *handle;
  String * device;		/* "hw:0", etc. */
  int useplug;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_stream_t mode;

  int rate;
  int channels;   
  snd_pcm_format_t format;	/* translate to/from string using formats[] */
  Audio_type atype;
  int format_bytes;

  /* frames_per_io determines the frequency of snd_pcm_readi calls. */
  snd_pcm_uframes_t frames_per_io;

  int enable;
  float total_dropped; 
  struct timespec t0;
  int analyze;
  int total_wait_for_it;
  uint64_t total_encoded_bytes;
  uint64_t total_encoded_next;
  uint64_t total_encoded_delta;
  double running_timestamp;
  
} ALSAio_common;

extern void ALSAio_open_playback(ALSAio_common * aic, const char * device, int rate, int channels, const char * format);
extern void ALSAio_open_capture(ALSAio_common * aic, const char * device, int rate, int channels, const char * format);
extern Audio_buffer * ALSAio_get_samples(ALSAio_common * aic);

#endif
