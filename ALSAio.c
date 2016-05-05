/* ALSA audio capture and playback. */

#include <string.h>		/* memcpy */
#include <dirent.h>		/* readdir */
#include <alsa/asoundlib.h>	/* ALSA library */
#include <math.h>		/* fabs */

#include "CTI.h"
#include "ALSAio.h"
#include "Wav.h"
#include "Audio.h"
#include "String.h"
#include "Mem.h"
#include "Cfg.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);
static void Wav_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_WAV, INPUT_AUDIO };
enum { OUTPUT_FEEDBACK };

static Input ALSAPlayback_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_WAV ] = { .type_label = "Wav_buffer", .handler = Wav_handler },
  // [ INPUT_AUDIO ] = { .type_label = "Audio_buffer", .handler = Audio_handler },
};
static Output ALSAPlayback_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L  },
};


enum { OUTPUT_WAV, OUTPUT_AUDIO };
static Input ALSACapture_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};
static Output ALSACapture_outputs[] = {
  [ OUTPUT_WAV ] = { .type_label = "Wav_buffer", .destination = 0L },
  [ OUTPUT_AUDIO ] = { .type_label = "Audio_buffer", .destination = 0L },
};


static int rates[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000, 192000 };

static int channels[] = { 
  1,				/* mono */
  2, 				/* stereo */
  3,				/* 2.1 */
  6,				/* 5.1 */
  12,				/* envy24 output (note: input is 10 channels) */
};

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

typedef struct {
  Instance i;
  ALSAio_common c;
} ALSAio_private;


/* Common/library functions. */
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

int ALSAio_set_format_string(ALSAio_common * aic, const char * format)
{
  int i;
  int rc = -1;
  for (i=0; i < table_size(formats); i++) {
    if (streq(formats[i].label, format)) {
      rc = snd_pcm_hw_params_set_format(aic->handle, aic->hwparams, formats[i].value);
      if (rc < 0) {
	fprintf(stderr, "*** snd_pcm_hw_params_set_format %s: %s\n", s(aic->device), snd_strerror(rc));
      }
      else {
	fprintf(stderr, "format set to %s\n", format);
	aic->format = formats[i].value;
	aic->atype = formats[i].atype;
	aic->format_bytes = formats[i].bytes;
      }
      break;
    }
  }  
  return rc;
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
    aic->enable = 0;
    return;
  }

  if (!aic->channels) {
    fprintf(stderr, "%s: error - channels is 0!\n", __func__);
    aic->enable = 0;
    return;
  }

  if (!aic->format_bytes) {
    fprintf(stderr, "%s: error - format_bytes is 0!\n", __func__);
    aic->enable = 0;
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
    getdoubletime(&aic->running_timestamp);
  }

  if (state != SND_PCM_STATE_PREPARED) {
    fprintf(stderr, "%s: error - wrong state %s\n", __func__, state_str[state]);
    aic->enable = 0;
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
    *bytes_transferred = (n * frames_to_transfer * aic->format_bytes);
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


static void get_device_range(Instance *pi, Range *range);


static int set_useplug(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  priv->c.useplug = atoi(value);
  fprintf(stderr, "ALSA useplug set to %d\n", priv->c.useplug);

  return 0;
}


static int set_device(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  int rc = 0;
  int i;

  String_free(&priv->c.device);
  priv->c.device = String_new(value);

  if (priv->c.handle) {
    snd_pcm_close(priv->c.handle);
  }

  /* Try matching by description first... */
  Range available_alsa_devices = {};
  get_device_range(pi, &available_alsa_devices);
  for (i=0; i < available_alsa_devices.descriptions.count; i++) {
    if (strstr(available_alsa_devices.descriptions.items[i]->bytes, value)) {
      puts("found it!");
      priv->c.device = String_new(available_alsa_devices.strings.items[i]->bytes);
      break;
    }
  }

  Range_clear(&available_alsa_devices);

  if (String_is_none(priv->c.device)) {
    /* Not found, try value as supplied. */
    priv->c.device = String_new(value);
  }
  
  rc = snd_pcm_open(&priv->c.handle, s(priv->c.device), priv->c.mode, 0);
  if (rc < 0) {
    fprintf(stderr, "*** snd_pcm_open %s: %s\n", s(priv->c.device), snd_strerror(rc));
    goto out;
  }
  
  fprintf(stderr, "ALSA device %s opened, handle=%p\n", s(priv->c.device), priv->c.handle);

  /* Allocate hardware parameter structure, and call "any", and use
     the resulting hwparams in subsequent calls.  I had tried calling
     _any() in each get/set function, but recording failed, so it
     seems ALSA doesn't work that way. */
  snd_pcm_hw_params_malloc(&priv->c.hwparams);
  rc = snd_pcm_hw_params_any(priv->c.handle, priv->c.hwparams);

  /* Might as well set interleaved here, too. */
  rc = snd_pcm_hw_params_set_access(priv->c.handle, priv->c.hwparams,
				    SND_PCM_ACCESS_RW_INTERLEAVED);
  if (rc != 0) {
    fprintf(stderr, "*** snd_pcm_hw_params_set_access %s: %s\n", s(priv->c.device), snd_strerror(rc));
  }

 out:
  return rc;
}


static void get_device_range(Instance *pi, Range *range)
{
  DIR *d;
  struct dirent *de;
  ALSAio_private *priv = (ALSAio_private *)pi;

  range->type = RANGE_STRINGS;

  d = opendir("/proc/asound");

  while (1) {
    de = readdir(d);
    if (!de) {
      closedir(d);
      break;
    }

    char c = de->d_name[strlen("card")];
    if (strncmp(de->d_name, "card", strlen("card")) == 0
	&& c >= '0' && c <= '9' ) {
      String *s;
      String *hw;
      String *desc;

      s = String_sprintf("/proc/asound/%s/id", de->d_name);
      if (priv->c.useplug) {
	hw = String_sprintf("plughw:%c", c);
      }
      else {
	hw = String_sprintf("hw:%c", c);
      }
      desc = File_load_text(s);

      fprintf(stderr, "%s %s -> %s", hw->bytes, s->bytes, desc->bytes);

      String_free(&s);

      ISet_add(range->strings, hw); hw = NULL;		/* ISet takes ownership. */
      ISet_add(range->descriptions, desc); desc = NULL;		/* ISet takes ownership. */
    }
  }
}


static int set_rate(Instance *pi, const char *value)
{
  int rc;
  unsigned int rate = atoi(value);
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }

  rc = snd_pcm_hw_params_set_rate(priv->c.handle,
				  priv->c.hwparams,
				  rate, 0);
  
  if (rc == 0) {
    fprintf(stderr, "rate set to %d\n", rate);
    priv->c.rate = rate;
  }
  else {
    fprintf(stderr, "*** error setting rate %d (%s)\n", rate, snd_strerror(rc));
    
  }

  return rc;
}


static void get_rate_range(Instance *pi, Range *range)
{
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  Range_clear(range);
  range->type = RANGE_INT_ENUM;

  for (i=0; i < table_size(rates); i++) {
    rc = snd_pcm_hw_params_test_rate(priv->c.handle, priv->c.hwparams, rates[i], 0);
    if (rc == 0) {
      fprintf(stderr, "  rate %d is available\n", rates[i]);
      Array_append(range->int_enums, rates[i]);
    }
  }
}


static int set_channels(Instance *pi, const char *value)
{
  int rc;
  int channels = atoi(value);
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }

  rc = snd_pcm_hw_params_set_channels(priv->c.handle,
				      priv->c.hwparams,
				      channels);
  
  if (rc == 0) {
    fprintf(stderr, "channels set to %d\n", channels);
    priv->c.channels = channels;
  }
  else {
    fprintf(stderr, "*** error setting %d channels\n", channels);
  }

  return rc;
}

static void get_channels_range(Instance *pi, Range *range)
{
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  Range_clear(range);
  range->type = RANGE_INT_ENUM;

  for (i=0; i < table_size(channels); i++) {
    rc = snd_pcm_hw_params_test_channels(priv->c.handle, priv->c.hwparams, channels[i]);
    if (rc == 0) {
      printf("  %d-channel sampling available\n", channels[i]);
      Array_append(range->int_enums, rates[i]);
    }
  }
}


static int set_format(Instance *pi, const char *value)
{
  int rc = -1;
  ALSAio_private *priv = (ALSAio_private *)pi;
  char tmp[strlen(value)+1];
  int j;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }
  
  strcpy(tmp, value);
  for (j=0; tmp[j]; j++) {
    if (tmp[j] == '.') {
      tmp[j] = ' ';
    }
  }

  rc = ALSAio_set_format_string(&priv->c, tmp);
  
  return rc;
}

static void get_format_range(Instance *pi, Range *range)
{
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  Range_clear(range);
  range->type = RANGE_STRINGS;

  for (i=0; i < table_size(formats); i++) {
    rc = snd_pcm_hw_params_test_format(priv->c.handle, priv->c.hwparams, formats[i].value);
    if (rc == 0) {
      fprintf(stderr, "  %s sampling available\n", formats[i].label);
      ISet_add(range->strings, String_new(formats[i].label));
    }
  }
}

static int set_frames_per_io(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    priv->c.enable = 0;
  }
  else {
    priv->c.frames_per_io = atol(value);
  }
  
  fprintf(stderr, "ALSACapture frames_per_io set to %ld\n", priv->c.frames_per_io);

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  if (!priv->c.handle) {
    fprintf(stderr, "*** device is not open!\n");
    priv->c.enable = 0;
  }
  else {
    priv->c.enable = atoi(value);
  }
  
  fprintf(stderr, "ALSA enable set to %d\n", priv->c.enable);

  return 0;
}


static Config config_table[] = {
  { "useplug",  set_useplug, 0L, 0L },
  { "device",  set_device, 0L, get_device_range },
  { "rate",  set_rate, 0L, get_rate_range },
  { "channels",  set_channels, 0L, get_channels_range },
  { "format",  set_format, 0L, get_format_range },
  { "frames_per_io",  set_frames_per_io, 0L, 0L},
  { "enable",  set_enable, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Wav_handler(Instance *pi, void *data)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  Wav_buffer *wav_in = data;
  int state;
  int dir = 0;
  int rc;
  int i;
  snd_pcm_sframes_t n;

  if (!priv->c.enable) {
    return;
  }

  if (!priv->c.rate) {
    /* Set rate. */
    char channels[32];
    char rate[32];
    sprintf(rate, "%d", wav_in->params.rate);
    set_rate(pi, rate);

    /* Set channels. */
    sprintf(channels, "%d", wav_in->params.channels);
    set_channels(pi, channels);

    /* Set format. */
    for (i=0; i < table_size(formats); i++) {
      if (formats[i].bytes * 8 == wav_in->params.bits_per_sample) {
	priv->c.format = formats[i].value;
	rc = snd_pcm_hw_params_set_format(priv->c.handle, priv->c.hwparams, priv->c.format);
	if (rc < 0) {
	  fprintf(stderr, "*** %s: snd_pcm_hw_params_set_format %s: %s\n", __func__,
		  s(priv->c.device), snd_strerror(rc));
	}
	break;
      }
    }
    
    if (i == table_size(formats)) {
      fprintf(stderr, "*** format for %d bits-per-sample not found!\n",
	      wav_in->params.bits_per_sample);
    }

  }

  state = snd_pcm_state(priv->c.handle);
  dpf("%s: state(1)=%s\n", __func__, state_str[state]);

  if (state == SND_PCM_STATE_OPEN || state == SND_PCM_STATE_SETUP) {
    /* One time playback setup. */

    /* FIXME: Why does 64 work on NVidia CK804 with "snd_intel8x0"
       driver, but not 32, 128, 2048?  How to find out what will
       work? */
    snd_pcm_uframes_t frames =  priv->c.frames_per_io;

    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    rc = snd_pcm_hw_params_set_period_size_near(priv->c.handle, priv->c.hwparams, &frames, &dir);
    fprintf(stderr, "set_period_size_near returns %d (frames=%d)\n", rc, (int)frames);

    int periods = 4;

    rc = snd_pcm_hw_params_set_periods(priv->c.handle, priv->c.hwparams, periods, 0);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params_set_periods %s: %s\n", s(priv->c.device), snd_strerror(rc));      
    }
      
    rc = snd_pcm_hw_params(priv->c.handle, priv->c.hwparams);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params %s: %s\n", s(priv->c.device), snd_strerror(rc));      
    }

    state = snd_pcm_state(priv->c.handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    rc = snd_pcm_prepare(priv->c.handle);
    state = snd_pcm_state(priv->c.handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);
  }

  dpf("%s: state(2)=%s\n", __func__, state_str[state]);

  int out_frames = wav_in->data_length / (priv->c.channels * priv->c.format_bytes);
  int frames_written = 0;
  while (1) {
    n = snd_pcm_writei(priv->c.handle, (uint8_t*)wav_in->data + (frames_written * (priv->c.channels * priv->c.format_bytes)),
		       out_frames);
    if (n > 0) {
      out_frames -= n;
      frames_written += n;
    }
    else {
      break;
    }
  }

  if (n < 0) {
    fprintf(stderr, "*** snd_pcm_writei %s: %s\n", s(priv->c.device), snd_strerror((int)n));
    fprintf(stderr, "*** attempting snd_pcm_prepare() to correct...\n");
    snd_pcm_prepare(priv->c.handle);
  }

  if (wav_in->no_feedback == 0 	/* The default is 0, set to 1 if "filler" code below is called. */
      && pi->outputs[OUTPUT_FEEDBACK].destination) {
    Feedback_buffer *fb = Feedback_buffer_new();
    fb->seq = wav_in->seq;
    PostData(fb, pi->outputs[OUTPUT_FEEDBACK].destination);
  }

  Wav_buffer_release(&wav_in);
}


static void ALSAPlayback_tick(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  // int wait_flag = (priv->c.enable ? 0 : 1);
  int wait_flag;

  if (!priv->c.enable
      || !priv->c.rate) {
    /* Not enabled, or rate not set, wait for an enable message or a
       Wav buffer.  This avoid spinning below. */
    wait_flag = 1;
  }
  else {
    /* Enabled and rate set, will generate filler if needed. */
    wait_flag = 0;
  }
  
  Handler_message *hm;

  hm = GetData(pi, wait_flag);
  // fprintf(stderr, "%s hm=%p\n", __func__, hm);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
    while (pi->pending_messages > 8) {
      /* This happens when capture clock is faster than playback
	 clock.  In cx88play.cmd, it drops a block every ~2s. */
      hm = GetData(pi, wait_flag);
      if (hm->handler == Wav_handler) {
	Wav_buffer *wav_in = hm->data;
	double s = (1.0*wav_in->data_length)/
	  (priv->c.rate * priv->c.channels * priv->c.format_bytes);
	priv->c.total_dropped += s;
	fprintf(stderr, "dropping %.4fs of audio (%d %d %d) total %.4f\n",
	       s, priv->c.rate, priv->c.channels, priv->c.format_bytes, priv->c.total_dropped);
	Wav_buffer_release(&wav_in);
      }
      else {
	/* Should always handle config messages... */
	hm->handler(pi, hm->data);
      }
      ReleaseMessage(&hm,pi);
    }
  }
  else if (priv->c.enable && priv->c.rate) {
    /* Filler... */
    Wav_buffer *wav_in;
    int i;
    //fprintf(stderr, "filler\n");
    wav_in = Wav_buffer_new(priv->c.rate, priv->c.channels, priv->c.format_bytes);
    wav_in->data_length = 16*priv->c.channels*priv->c.format_bytes;
    wav_in->data = Mem_calloc(1, wav_in->data_length);
    if (access("/dev/shm/a1", R_OK) == 0) {
      /* Blip. */
      for (i=0; i < wav_in->data_length; i+=2) {
	//wav_in->data[i] = 0;
	//wav_in->data[i+1] = (2048 - i);
      }
      unlink("/dev/shm/a1");
    }
    Wav_buffer_finalize(wav_in);
    /* Avoid sending feedback since this was generated locally. */
    wav_in->no_feedback = 1;
    Wav_handler(pi, wav_in);
  }
  else {
    /* Should never get here. */
    fprintf(stderr, "WTF?\n");
    sleep(1);
  }
   

}


static void analyze_rate(ALSAio_private *priv, Wav_buffer *wav)
{
  if (priv->c.total_wait_for_it) {
    priv->c.total_wait_for_it -= 1;
  }
  else {
    if (priv->c.t0.tv_sec == 0) {
      clock_gettime(CLOCK_MONOTONIC, &priv->c.t0);
    }
    if (priv->c.total_encoded_bytes > priv->c.total_encoded_next) {
      struct timespec tnow;
      double d;
      clock_gettime(CLOCK_MONOTONIC, &tnow);
      d = (tnow.tv_sec + tnow.tv_nsec/1000000000.0) -
	(priv->c.t0.tv_sec + priv->c.t0.tv_nsec/1000000000.0);
      fprintf(stderr, "encoding %.4f bytes/sec\n", (priv->c.total_encoded_bytes/d));
      priv->c.total_encoded_next += priv->c.total_encoded_delta;
    }
    priv->c.total_encoded_bytes += wav->data_length;
  }
}


static void ALSACapture_tick(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  int wait_flag;

  if (!priv->c.enable || !priv->c.handle) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  Handler_message *hm;

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->c.enable || !priv->c.handle) {
    /* Not enabled or no handle, don't try to capture anything. */
    return;
  }
 
  /* Read a block of data. */
  int rc;
  snd_pcm_sframes_t n;
  int state;
  snd_pcm_uframes_t frames = priv->c.frames_per_io;
  int dir = 0;

  state = snd_pcm_state(priv->c.handle);

  dpf("%s: state(1)=%s\n", __func__, state_str[state]);

  if (state == SND_PCM_STATE_OPEN || state == SND_PCM_STATE_SETUP) {
    /* One time capture setup. */
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    rc = snd_pcm_hw_params_set_period_size_near(priv->c.handle, priv->c.hwparams, &frames, &dir);
    fprintf(stderr, "%s: set_period_size_near returns %d (frames %d:%d)\n", 
	    __func__, rc, (int)priv->c.frames_per_io, (int)frames);

    rc = snd_pcm_hw_params(priv->c.handle, priv->c.hwparams);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params %s: %s\n", s(priv->c.device), snd_strerror(rc));      
    }

    state = snd_pcm_state(priv->c.handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    rc = snd_pcm_prepare(priv->c.handle);
    state = snd_pcm_state(priv->c.handle);
    fprintf(stderr, "%s: state=%s\n", __func__, state_str[state]);

    /* Initialize running_timestamp. */
    getdoubletime(&priv->c.running_timestamp);
  }

  snd_pcm_hw_params_get_period_size(priv->c.hwparams, &frames, &dir);
  int size = frames * priv->c.format_bytes * priv->c.channels;

  if (!size) {
    // This can happen if channels or format_bytes was not set...
    fprintf(stderr, "%s: size error - %ld * %d * %d\n", __func__, frames , priv->c.format_bytes , priv->c.channels);
    while (1) {
      sleep(1);
    }
  }

  if (!priv->c.rate) {
    fprintf(stderr, "%s: error - rate is 0!\n", __func__);
    while (1) {
      sleep(1);
    }
  }

  /* Allocate buffer for PCM samples. */
  uint8_t *buffer = Mem_malloc(size+1);
  buffer[size] = 0x55;

  //int val = hwparams->rate;
  //snd_pcm_hw_params_get_period_time(params, &val, &dir);

  /* Read the data. */
  n = snd_pcm_readi(priv->c.handle, buffer, frames);

  if (buffer[size] != 0x55)  {
    fprintf(stderr, "*** overwrote audio buffer!\n");
  }

  if (n != frames) {
    fprintf(stderr, "*** snd_pcm_readi %s: %s\n", s(priv->c.device), snd_strerror((int)n));
    fprintf(stderr, "*** attempting snd_pcm_prepare() to correct...\n");
    snd_pcm_prepare(priv->c.handle);
    goto out;
  }

  // fprintf(stderr, "*** read %d frames\n", (int) n);
  
  double calculated_period = frames*1.0/(priv->c.rate);
  
  priv->c.running_timestamp += calculated_period;
  
  double tnow;
  getdoubletime(&tnow);
  
  /* Do coarse adjustment if necessary, this can happen after a
     system date change via ntp or htpdate. */
  if (fabs(priv->c.running_timestamp - tnow) > 5.0) {
    fprintf(stderr, "coarse timestamp adjustment, %.3f -> %.3f\n",
	    priv->c.running_timestamp, tnow);
    priv->c.running_timestamp = tnow;
  }
  
  /* Adjust running timestamp if it slips too far either way.  Smoothing, I guess. */
  if (priv->c.running_timestamp - tnow > calculated_period) {
    fprintf(stderr, "- running timestamp\n");
    fprintf(stderr, "priv->c.rate=%d,  %.3f - %.3f > %.5f : - running timestamp\n", 
	    priv->c.rate, 
	    priv->c.running_timestamp , tnow , calculated_period);
    priv->c.running_timestamp -= (calculated_period/2.0);      
  }
  else if (tnow - priv->c.running_timestamp > calculated_period) {
    fprintf(stderr, "priv->c.rate=%d, %.3f - %.3f > %.5f : + running timestamp\n",
	    priv->c.rate, 
	    tnow , priv->c.running_timestamp , calculated_period);
    priv->c.running_timestamp += (calculated_period/2.0);
  }
  
  int buffer_bytes = n * priv->c.format_bytes * priv->c.channels;
  
  if (pi->outputs[OUTPUT_AUDIO].destination) {
    Audio_buffer * audio = Audio_buffer_new(priv->c.rate,
					    priv->c.channels, priv->c.atype);
    Audio_buffer_add_samples(audio, buffer, buffer_bytes);
    audio->timestamp = priv->c.running_timestamp;
    //fprintf(stderr, "posting audio buffer %d bytes\n", buffer_bytes);
    PostData(audio, pi->outputs[OUTPUT_AUDIO].destination);
    /* TESTING: disable after posting once */
    //pi->outputs[OUTPUT_AUDIO].destination = NULL;
  }
  
  if (pi->outputs[OUTPUT_WAV].destination) {
    Wav_buffer *wav = Wav_buffer_new(priv->c.rate, priv->c.channels, priv->c.format_bytes);
    wav->timestamp = priv->c.running_timestamp;
    
    dpf("%s allocated wav @ %p\n", __func__, wav);
    wav->data = buffer;  buffer = 0L;	/* Assign, do not free below. */
    wav->data_length = buffer_bytes;
    Wav_buffer_finalize(wav);
    
    if (priv->c.analyze) {
      analyze_rate(priv, wav);
    }
    
    PostData(wav, pi->outputs[OUTPUT_WAV].destination);
    static int x = 0;
    wav->seq = x++;
    wav = 0L;
  }

 out:
  if (buffer) {
    Mem_free(buffer);
  }
}

/* Playback init... */
static void ALSAPlayback_instance_init(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  priv->c.frames_per_io = 64;
  priv->c.mode = SND_PCM_STREAM_PLAYBACK;
  
}

static Template ALSAPlayback_template = {
  .label = "ALSAPlayback",
  .priv_size = sizeof(ALSAio_private),
  .inputs = ALSAPlayback_inputs,
  .num_inputs = table_size(ALSAPlayback_inputs),
  .outputs = ALSAPlayback_outputs,
  .num_outputs = table_size(ALSAPlayback_outputs),
  .tick = ALSAPlayback_tick,
  .instance_init = ALSAPlayback_instance_init,
};

/* Capture init... */
static void ALSACapture_instance_init(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  priv->c.frames_per_io = 64;
  priv->c.mode = SND_PCM_STREAM_CAPTURE;
  priv->c.total_encoded_delta = 48000*2*2*1;
  priv->c.total_encoded_next = priv->c.total_encoded_delta;
  priv->c.total_wait_for_it = 1000;
  
}

static Template ALSACapture_template = {
  .label = "ALSACapture",
  .priv_size = sizeof(ALSAio_private),
  .inputs = ALSACapture_inputs,
  .num_inputs = table_size(ALSACapture_inputs),
  .outputs = ALSACapture_outputs,
  .num_outputs = table_size(ALSACapture_outputs),
  .tick = ALSACapture_tick,
  .instance_init = ALSACapture_instance_init,
};



void ALSAio_init(void)
{
  Template_register(&ALSACapture_template);
  Template_register(&ALSAPlayback_template);
}

shared_export(ALSAio_init);

