#include "ALSAio.h"
#include "cti_utils.h"

/* Note that formats table here does *not* define number of channels. */
static struct {
  const char *label;
  snd_pcm_format_t value;
  Audio_type atype;
  int bytes;
} formats[] = {
  { .label = "signed 8-bit", .value = SND_PCM_FORMAT_S8, .atype = CTI_AUDIO_8BIT_SIGNED_LE, .bytes = 1 },
  { .label = "signed 16-bit little endian", .value = SND_PCM_FORMAT_S16_LE, .atype = CTI_AUDIO_16BIT_SIGNED_LE, .bytes = 2 },
  { .label = "signed 24-bit little endian", .value = SND_PCM_FORMAT_S24_LE, .atype = CTI_AUDIO_24BIT_SIGNED_LE, .bytes = 3 },
  { .label = "signed 32-bit little endian", .value = SND_PCM_FORMAT_S32_LE, .atype = CTI_AUDIO_32BIT_SIGNED_LE, .bytes = 4 },
};

/* Common/library functions. */
int ALSAio_set_format_string(ALSAio_common * aic, const char * format)
{
  int i;
  int rc = -1;
  for (i=0; i < cti_table_size(formats); i++) {
    if (streq(formats[i].label, format)) {
      rc = snd_pcm_hw_params_set_format(aic->handle, aic->hwparams, formats[i].value);
      if (rc < 0) {
	fprintf(stderr, "*** snd_pcm_hw_params_set_format %s: %s\n", s(aic->device), snd_strerror(rc));
      }
      else {
	aic->format = formats[i].value;
	aic->atype = formats[i].atype;
	aic->format_bytes = formats[i].bytes;
	fprintf(stderr, "format set to %s\n", format);
      }
      break;
    }
  }  
  return rc;
}

static void ALSAio_open_common(ALSAio_common * aic, const char * device, int rate, int channels, const char * format,
			       snd_pcm_stream_t mode)
{
  int rc;

  String_free(&aic->device);
  aic->device = String_new(device);
  aic->rate = rate;
  aic->channels = channels;
  aic->mode = mode;

  if (aic->handle) {
    fprintf(stderr, "%s: handle should be initialized NULL\n", __func__);
    return;
  }

  rc = snd_pcm_open(&aic->handle, device, aic->mode, 0);
  if (rc < 0) {
    fprintf(stderr, "*** snd_pcm_open %s: %s\n", device, snd_strerror(rc));
    return;
  }

  snd_pcm_hw_params_malloc(&aic->hwparams);
  rc = snd_pcm_hw_params_any(aic->handle, aic->hwparams);
  if (rc < 0) {
    fprintf(stderr, "*** snd_pcm_hw_params_any: %s %s\n", device, snd_strerror(rc));
  }

  rc = snd_pcm_hw_params_set_access(aic->handle, aic->hwparams,
				    SND_PCM_ACCESS_RW_INTERLEAVED);

  if (rc < 0) {
    fprintf(stderr, "*** snd_pcm_hw_set_access: %s %s\n", device, snd_strerror(rc));
  }

  rc = snd_pcm_hw_params_set_rate(aic->handle, aic->hwparams, aic->rate, 0);
  if (rc == 0) {
    fprintf(stderr, "rate set to %d\n", rate);
  }
  else {
    fprintf(stderr, "*** error setting rate %d (%s)\n", rate, snd_strerror(rc));
  }
  
  rc = snd_pcm_hw_params_set_channels(aic->handle, aic->hwparams, channels);
  
  if (rc == 0) {
    fprintf(stderr, "channels set to %d\n", channels);
    aic->channels = channels;
  }
  else {
    fprintf(stderr, "*** error setting channels %d (%s)\n", channels, snd_strerror(rc));
  }

  rc = ALSAio_set_format_string(aic, format);
}

void ALSAio_open_playback(ALSAio_common * aic, const char * device, int rate, int channels, const char * format)
{
  ALSAio_open_common(aic, device, rate, channels, format, SND_PCM_STREAM_PLAYBACK);
}

void ALSAio_open_capture(ALSAio_common * aic, const char * device, int rate, int channels, const char * format)
{
  ALSAio_open_common(aic, device, rate, channels, format, SND_PCM_STREAM_CAPTURE); 
}

static const char * state_str[] = {
  [SND_PCM_STATE_OPEN] = "SND_PCM_STATE_OPEN",
  [SND_PCM_STATE_SETUP] = "SND_PCM_STATE_SETUP",
  [SND_PCM_STATE_PREPARED] = "SND_PCM_STATE_PREPARED",
  [SND_PCM_STATE_RUNNING] = "SND_PCM_STATE_RUNNING",
  [SND_PCM_STATE_XRUN] = "SND_PCM_STATE_XRUN",
  [SND_PCM_STATE_DRAINING] = "SND_PCM_STATE_DRAINING",
  [SND_PCM_STATE_PAUSED] = "SND_PCM_STATE_PAUSED",
  [SND_PCM_STATE_SUSPENDED] = "SND_PCM_STATE_SUSPENDED",
  [SND_PCM_STATE_DISCONNECTED] = "SND_PCM_STATE_DISCONNECTED",
};

const char * ALSAio_state_to_string(int state)
{
  if (state < 0 || state > SND_PCM_STATE_LAST) {
    return "UNKNOWN_STATE";
  }
  return state_str[state];
}

static void ALSA_buffer_io(ALSAio_common * aic, 
			   uint8_t * buffer,
			   int buffer_size,
			   int * bytes_transferred,
			   ALSAio_rw_enum rw)
{
  int rc;
  snd_pcm_sframes_t n;
  int state;
  int dir = 0;			/* Returned by *_near() calls. */

  *bytes_transferred = 0;	/* Start with 0, for simple return on error. */

  if (!aic->rate) {
    fprintf(stderr, "%s: error - rate is 0!\n", __func__);
    return;
  }

  if (!aic->channels) {
    fprintf(stderr, "%s: error - channels is 0!\n", __func__);
    return;
  }

  if (!aic->format_bytes) {
    fprintf(stderr, "%s: error - format_bytes is 0!\n", __func__);
    return;
  }

  state = snd_pcm_state(aic->handle);

  if (state == SND_PCM_STATE_OPEN || state == SND_PCM_STATE_SETUP) {
    /* One time setup. */
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);
    snd_pcm_uframes_t frames = aic->frames_per_io;

    rc = snd_pcm_hw_params_set_period_size_near(aic->handle, aic->hwparams, &frames, &dir);
    fprintf(stderr, "%s: set_period_size_near returns %d (frames %d -> %d) dir=%d\n", 
	    __func__, rc, (int)aic->frames_per_io, (int)frames, dir);
    aic->frames_per_io = frames;

    rc = snd_pcm_hw_params(aic->handle, aic->hwparams);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params %s: %s\n", s(aic->device), snd_strerror(rc));      
    }

    state = snd_pcm_state(aic->handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    rc = snd_pcm_prepare(aic->handle);
    state = snd_pcm_state(aic->handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    /* Initialize running_timestamp. */
    cti_getdoubletime(&aic->running_timestamp);
  }

  if (!(state == SND_PCM_STATE_PREPARED || state == SND_PCM_STATE_RUNNING)) {
    fprintf(stderr, "%s: error - wrong state %s\n", __func__, state_str[state]);
    return;
  }

  snd_pcm_uframes_t frames_to_transfer = 0;

  if (rw == ALSAIO_READ) {
    rc = snd_pcm_hw_params_get_period_size(aic->hwparams, &frames_to_transfer, &dir);
    if (rc != 0) {
      fprintf(stderr, "*** %s: %s\n", __func__, snd_strerror(rc));
      return;
    }

    if (frames_to_transfer * aic->format_bytes * aic->channels > buffer_size) {
      fprintf(stderr, "*** %s: buffer_size %d is too small!\n",
	      __func__, buffer_size);
      return;
    }
    n = snd_pcm_readi(aic->handle, buffer, frames_to_transfer);
  }
  else if (rw == ALSAIO_WRITE) {
    frames_to_transfer = buffer_size/(aic->format_bytes * aic->channels);;
    /* Using blocking mode. */
    n = snd_pcm_writei(aic->handle, buffer, frames_to_transfer);
  }

  if (n != frames_to_transfer) {
    fprintf(stderr, "*** snd_pcm_%s %s: %s\n",
	    rw == ALSAIO_READ ? "readi":"writei",
	    s(aic->device), 
	    snd_strerror((int)n));
    fprintf(stderr, "*** attempting snd_pcm_prepare() to correct...\n");
    snd_pcm_prepare(aic->handle);
  }
  else {
    *bytes_transferred = (n * aic->format_bytes * aic->channels);
    // fprintf(stderr, "%02x %02x %02x %02x...\n", buffer[0], buffer[1], buffer[2], buffer[3]);
  }
}


Audio_buffer * ALSAio_get_samples(ALSAio_common * aic)
{
  Audio_buffer * ab = NULL;
  uint8_t buffer[32768];
  int transferred = 0;
  ALSA_buffer_io(aic, buffer, sizeof(buffer), &transferred, ALSAIO_READ);
  if (transferred) {
    ab = Audio_buffer_new(aic->rate, aic->channels, aic->atype);
    Audio_buffer_add_samples(ab, buffer, transferred);
  }
  return ab;
}

snd_pcm_format_t ALSAio_bps_to_snd_fmt(int bits_per_sample)
{
  int i;
  
  for (i=0; i < cti_table_size(formats); i++) {
    if (formats[i].bytes * 8 == bits_per_sample) {
      return formats[i].value;
    }
  }
  
  fprintf(stderr, "*** format for %d bits-per-sample not found!\n", bits_per_sample);  
  return SND_PCM_FORMAT_UNKNOWN;
}
