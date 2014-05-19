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


#if 0
/* FIXME: Use these when ready. */
static int rates[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000, 192000 };

static int channels[] = { 
  1,				/* mono */
  2, 				/* stereo */
  3,				/* 2.1 */
  6,				/* 5.1 */
  12,				/* envy24 output (note: input is 10 channels) */
};
#endif

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
  snd_pcm_t *handle;
  String device;			/* "hw:0", etc. */
  int useplug;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_stream_t mode;

  int rate;
  int channels;   
  snd_pcm_format_t format;	/* can translate to string using formats[] */
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

} ALSAio_private;


static void get_device_range(Instance *pi, Range *range);


static int set_useplug(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  priv->useplug = atoi(value);
  printf("ALSA useplug set to %d\n", priv->useplug);

  return 0;
}


static int set_device(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  int rc = 0;
  int i;

  String_set(&priv->device, value);

  if (priv->handle) {
    snd_pcm_close(priv->handle);
  }

  /* Try matching by description first... */
  Range available_alsa_devices = {};
  get_device_range(pi, &available_alsa_devices);
  for (i=0; i < available_alsa_devices.descriptions.count; i++) {
    if (strstr(available_alsa_devices.descriptions.items[i]->bytes, value)) {
      puts("found it!");
      String_set(&priv->device, available_alsa_devices.strings.items[i]->bytes);
      break;
    }
  }

  Range_free(&available_alsa_devices);

  if (!sl(priv->device)) {
    /* Not found, try value as supplied. */
    String_set(&priv->device, value);
  }
  
  rc = snd_pcm_open(&priv->handle, sl(priv->device), priv->mode, 0);
  if (rc < 0) {
    fprintf(stderr, "*** snd_pcm_open %s: %s\n", sl(priv->device), snd_strerror(rc));
    goto out;
  }
  
  printf("ALSA device %s opened, handle=%p\n", sl(priv->device), priv->handle);

  /* Allocate hardware parameter structure, and call "any", and use
     the resulting hwparams in subsequent calls.  I had tried calling
     _any() in each get/set function, but recording failed, so it
     seems ALSA doesn't work that way. */
  snd_pcm_hw_params_malloc(&priv->hwparams);
  rc = snd_pcm_hw_params_any(priv->handle, priv->hwparams);

  /* Might as well set interleaved here, too. */
  rc = snd_pcm_hw_params_set_access(priv->handle, priv->hwparams,
				    SND_PCM_ACCESS_RW_INTERLEAVED);
  if (rc != 0) {
    fprintf(stderr, "*** snd_pcm_hw_params_set_access %s: %s\n", sl(priv->device), snd_strerror(rc));
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
      if (priv->useplug) {
	hw = String_sprintf("plughw:%c", c);
      }
      else {
	hw = String_sprintf("hw:%c", c);
      }
      desc = File_load_text(s);

      printf("%s %s -> %s\n", hw->bytes, s->bytes, desc->bytes);

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

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }

  rc = snd_pcm_hw_params_set_rate(priv->handle,
				  priv->hwparams,
				  rate, 0);
  
  if (rc == 0) {
    printf("rate set to %d\n", rate);
    priv->rate = rate;
  }
  else {
    fprintf(stderr, "*** error setting rate %d (%s)\n", rate, snd_strerror(rc));
    
  }

  return rc;
}


static void get_rate_range(Instance *pi, Range *range)
{
#if 0
  /* FIXME: Need modc or similar ephemeral string and list support. */
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  for (i=0; i < table_size(rates); i++) {
    rc = snd_pcm_hw_params_test_rate(priv->handle, priv->hwparams, rates[i], 0);
    if (rc == 0) {
      printf("  rate %d is available\n", rates[i]);
    }
  }
#endif
}


static int set_channels(Instance *pi, const char *value)
{
  int rc;
  int channels = atoi(value);
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }

  rc = snd_pcm_hw_params_set_channels(priv->handle,
				      priv->hwparams,
				      channels);
  
  if (rc == 0) {
    printf("channels set to %d\n", channels);
    priv->channels = channels;
  }
  else {
    fprintf(stderr, "*** error setting %d channels\n", channels);
  }

  return rc;
}

static void get_channels_range(Instance *pi, Range *range)
{
#if 0
  /* FIXME: Need modc or similar ephemeral string and list support. */
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  for (i=0; i < table_size(channels); i++) {
    rc = snd_pcm_hw_params_test_channels(priv->handle, priv->hwparams, channels[i]);
    if (rc == 0) {
      printf("  %d-channel sampling available\n", channels[i]);
    }
  }
#endif
}


static int set_format(Instance *pi, const char *value)
{
  int i;
  int rc = -1;
  ALSAio_private *priv = (ALSAio_private *)pi;
  char tmp[strlen(value)+1];
  int j;

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return 1;
  }
  
  strcpy(tmp, value);
  for (j=0; tmp[j]; j++) {
    if (tmp[j] == '.') {
      tmp[j] = ' ';
    }
  }

  for (i=0; i < table_size(formats); i++) {
    if (streq(formats[i].label, tmp)) {
      rc = snd_pcm_hw_params_set_format(priv->handle, priv->hwparams, formats[i].value);
      if (rc < 0) {
	fprintf(stderr, "*** snd_pcm_hw_params_set_format %s: %s\n", sl(priv->device), snd_strerror(rc));
      }
      else {
	printf("format set to %s\n", value);
	priv->format = formats[i].value;
	priv->atype = formats[i].atype;
	priv->format_bytes = formats[i].bytes;
      }
      break;
    }
  }  
  
  return rc;
}

static void get_format_range(Instance *pi, Range *range)
{
#if 0
  /* FIXME: Need modc or similar ephemeral string and list support. */
  int i;
  int rc;
  ALSAio_private *priv = (ALSAio_private *)pi;

  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    return;
  }

  for (i=0; i < table_size(formats); i++) {
    rc = snd_pcm_hw_params_test_format(priv->handle, priv->hwparams, formats[i].value);
    if (rc == 0) {
      printf("  %s sampling available\n", formats[i].label);
    }
  }
#endif
}

static int set_frames_per_io(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    priv->enable = 0;
  }
  else {
    priv->frames_per_io = atol(value);
  }
  
  printf("ALSACapture frames_per_io set to %ld\n", priv->frames_per_io);

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  if (!priv->handle) {
    fprintf(stderr, "*** device is not open!\n");
    priv->enable = 0;
  }
  else {
    priv->enable = atoi(value);
  }
  
  printf("ALSA enable set to %d\n", priv->enable);

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

  if (!priv->enable) {
    return;
  }

  if (!priv->rate) {
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
	priv->format = formats[i].value;
	rc = snd_pcm_hw_params_set_format(priv->handle, priv->hwparams, priv->format);
	if (rc < 0) {
	  fprintf(stderr, "*** %s: snd_pcm_hw_params_set_format %s: %s\n", __func__,
		  sl(priv->device), snd_strerror(rc));
	}
	break;
      }
    }
    
    if (i == table_size(formats)) {
      fprintf(stderr, "*** format for %d bits-per-sample not found!\n",
	      wav_in->params.bits_per_sample);
    }

  }

  state = snd_pcm_state(priv->handle);

  if (state == SND_PCM_STATE_OPEN || state == SND_PCM_STATE_SETUP) {
    /* One time playback setup. */

    /* FIXME: Why does 64 work on NVidia CK804 with "snd_intel8x0"
       driver, but not 32, 128, 2048?  How to find out what will
       work? */
    snd_pcm_uframes_t frames =  priv->frames_per_io;

    printf("state=%d\n", state);

    rc = snd_pcm_hw_params_set_period_size_near(priv->handle, priv->hwparams, &frames, &dir);
    printf("set_period_size_near returns %d (frames=%d)\n", rc, (int)frames);

    int periods = 4;

    rc = snd_pcm_hw_params_set_periods(priv->handle, priv->hwparams, periods, 0);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params_set_periods %s: %s\n", sl(priv->device), snd_strerror(rc));      
    }
      
    rc = snd_pcm_hw_params(priv->handle, priv->hwparams);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params %s: %s\n", sl(priv->device), snd_strerror(rc));      
    }

    state = snd_pcm_state(priv->handle);
    printf("state=%d\n", state);

    rc = snd_pcm_prepare(priv->handle);
    state = snd_pcm_state(priv->handle);
    printf("state=%d\n", state);
  }

  int out_frames = wav_in->data_length / (priv->channels * priv->format_bytes);
  int frames_written = 0;
  while (1) {
    n = snd_pcm_writei(priv->handle, (uint8_t*)wav_in->data + (frames_written * (priv->channels * priv->format_bytes)),
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
    fprintf(stderr, "*** snd_pcm_writei %s: %s\n", sl(priv->device), snd_strerror((int)n));
    fprintf(stderr, "*** attempting snd_pcm_prepare() to correct...\n");
    snd_pcm_prepare(priv->handle);
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
  // int wait_flag = (priv->enable ? 0 : 1);
  int wait_flag;

  if (!priv->enable
      || !priv->rate) {
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
  // printf("%s hm=%p\n", __func__, hm);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
    while (pi->pending_messages > 4) {
      /* This happens when capture clock is faster than playback
	 clock.  In cx88play.cmd, it drops a block every ~2s. */
      hm = GetData(pi, wait_flag);
      if (hm->handler == Wav_handler) {
	Wav_buffer *wav_in = hm->data;
	double s = (1.0*wav_in->data_length)/
	  (priv->rate * priv->channels * priv->format_bytes);
	priv->total_dropped += s;
	printf("dropping %.4fs of audio (%d %d %d) total %.4f\n",
	       s, priv->rate, priv->channels, priv->format_bytes, priv->total_dropped);
	Wav_buffer_release(&wav_in);
      }
      else {
	/* Should always handle config messages... */
	hm->handler(pi, hm->data);
      }
      ReleaseMessage(&hm,pi);
    }
  }
  else if (priv->enable && priv->rate) {
    /* Filler... */
    Wav_buffer *wav_in;
    int i;
    //fprintf(stderr, "filler\n");
    wav_in = Wav_buffer_new(priv->rate, priv->channels, priv->format_bytes);
    wav_in->data_length = 16*priv->channels*priv->format_bytes;
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
    printf("WTF?\n");
    sleep(1);
  }
   

}


static void analyze_rate(ALSAio_private *priv, Wav_buffer *wav)
{
  if (priv->total_wait_for_it) {
    priv->total_wait_for_it -= 1;
  }
  else {
    if (priv->t0.tv_sec == 0) {
      clock_gettime(CLOCK_MONOTONIC, &priv->t0);
    }
    if (priv->total_encoded_bytes > priv->total_encoded_next) {
      struct timespec tnow;
      double d;
      clock_gettime(CLOCK_MONOTONIC, &tnow);
      d = (tnow.tv_sec + tnow.tv_nsec/1000000000.0) -
	(priv->t0.tv_sec + priv->t0.tv_nsec/1000000000.0);
      printf("encoding %.4f bytes/sec\n", (priv->total_encoded_bytes/d));
      priv->total_encoded_next += priv->total_encoded_delta;
    }
    priv->total_encoded_bytes += wav->data_length;
  }
}


static void ALSACapture_tick(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  int wait_flag;

  if (!priv->enable || !priv->handle) {
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

  if (!priv->enable || !priv->handle) {
    /* Not enabled or no handle, don't try to capture anything. */
    return;
  }
 
  /* Read a block of data. */
  int rc;
  snd_pcm_sframes_t n;
  int state;
  snd_pcm_uframes_t frames = priv->frames_per_io;
  int dir = 0;

  state = snd_pcm_state(priv->handle);

  if (state == SND_PCM_STATE_OPEN || state == SND_PCM_STATE_SETUP) {
    /* One time capture setup. */
    printf("%s: state=%d\n", __func__, state);

    rc = snd_pcm_hw_params_set_period_size_near(priv->handle, priv->hwparams, &frames, &dir);
    printf("%s: set_period_size_near returns %d (frames=%d)\n", __func__, rc, (int)frames);

    rc = snd_pcm_hw_params(priv->handle, priv->hwparams);
    if (rc < 0) {
      fprintf(stderr, "*** snd_pcm_hw_params %s: %s\n", sl(priv->device), snd_strerror(rc));      
    }

    state = snd_pcm_state(priv->handle);
    printf("%s: state=%d\n", __func__, state);

    rc = snd_pcm_prepare(priv->handle);
    state = snd_pcm_state(priv->handle);
    printf("%s: state=%d\n", __func__, state);

    /* Initialize running_timestamp. */
    getdoubletime(&priv->running_timestamp);
  }

  snd_pcm_hw_params_get_period_size(priv->hwparams, &frames, &dir);
  int size = frames * priv->format_bytes * priv->channels;

  if (!size) {
    // This can happen if channels or format_bytes was not set...
    printf("%s: size error - %ld * %d * %d\n", __func__, frames , priv->format_bytes , priv->channels);
    while (1) {
      sleep(1);
    }
  }

  if (!priv->rate) {
    printf("%s: error - rate is 0!\n", __func__);
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
  n = snd_pcm_readi(priv->handle, buffer, frames);

  if (buffer[size] != 0x55)  {
    fprintf(stderr, "*** overwrote audio buffer!\n");
  }

  if (n != frames) {
    fprintf(stderr, "*** snd_pcm_readi %s: %s\n", sl(priv->device), snd_strerror((int)n));
    fprintf(stderr, "*** attempting snd_pcm_prepare() to correct...\n");
    snd_pcm_prepare(priv->handle);
  }
  else {
    // fprintf(stderr, "*** read %d frames\n", (int) n);

    double calculated_period = frames*1.0/(priv->rate);

    priv->running_timestamp += calculated_period;
    
    double tnow;
    getdoubletime(&tnow);

    /* Do coarse adjustment if necessary, this can happen after a
       system date change via ntp or htpdate. */
    if (fabs(priv->running_timestamp - tnow) > 5.0) {
      printf("coarse timestamp adjustment, %.3f -> %.3f\n",
	     priv->running_timestamp, tnow);
      priv->running_timestamp = tnow;
    }

    /* Adjust running timestamp if it slips too far either way.  Smoothing, I guess. */
    if (priv->running_timestamp - tnow > calculated_period) {
      printf("- running timestamp\n");
      printf("priv->rate=%d,  %.3f - %.3f > %.5f : - running timestamp\n", 
	     priv->rate, 
	     priv->running_timestamp , tnow , calculated_period);
      priv->running_timestamp -= (calculated_period/2.0);      
    }
    else if (tnow - priv->running_timestamp > calculated_period) {
      printf("priv->rate=%d, %.3f - %.3f > %.5f : + running timestamp\n",
	     priv->rate, 
	     tnow , priv->running_timestamp , calculated_period);
      priv->running_timestamp += (calculated_period/2.0);
    }

    int buffer_bytes = n * priv->format_bytes * priv->channels;
    if (pi->outputs[OUTPUT_AUDIO].destination) {
      Audio_buffer * audio = Audio_buffer_new(priv->rate,
					      priv->channels, priv->atype);
      Audio_buffer_add_samples(audio, buffer, buffer_bytes);
      audio->timestamp = priv->running_timestamp;
      printf("posting audio buffer %d bytes\n", buffer_bytes);
      PostData(audio, pi->outputs[OUTPUT_AUDIO].destination);
      /* TESTING: disable after posting once */
      //pi->outputs[OUTPUT_AUDIO].destination = NULL;
    }

    if (pi->outputs[OUTPUT_WAV].destination) {
      Wav_buffer *wav = Wav_buffer_new(priv->rate, priv->channels, priv->format_bytes);
      wav->timestamp = priv->running_timestamp;

      dpf("%s allocated wav @ %p\n", __func__, wav);
      wav->data = buffer;  buffer = 0L;	/* Assign, do not free below. */
      wav->data_length = buffer_bytes;
      Wav_buffer_finalize(wav);

      if (priv->analyze) {
	analyze_rate(priv, wav);
      }

      PostData(wav, pi->outputs[OUTPUT_WAV].destination);
      static int x = 0;
      wav->seq = x++;
      wav = 0L;
    }
  }
  
  if (buffer) {
    Mem_free(buffer);
  }
}

/* Playback init... */
static void ALSAPlayback_instance_init(Instance *pi)
{
  ALSAio_private *priv = (ALSAio_private *)pi;
  priv->frames_per_io = 64;
  priv->mode = SND_PCM_STREAM_PLAYBACK;
  
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
  priv->frames_per_io = 64;
  priv->mode = SND_PCM_STREAM_CAPTURE;
  priv->total_encoded_delta = 48000*2*2*1;
  priv->total_encoded_next = priv->total_encoded_delta;
  priv->total_wait_for_it = 1000;
  
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
