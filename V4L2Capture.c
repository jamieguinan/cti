/* 
 * V4L2 capture.  This has been tested with:
 *   WinTV (BTTV)
 *   pcHDTV
 *   Logitech 9000, C310
 *   ov511
 *   vimicro
 */

#include <stdio.h>
#include <string.h>
// extern char *strdup(const char *s); /* why not getting this from string.h??  Oh, -std=c99... */
#define streq(a, b)  (strcmp(a, b) == 0)
#include <stdlib.h>
#include <sys/ioctl.h>		/* ioctl */
#include <sys/mman.h>		/* mmap */
#include <poll.h>		/* poll */

/* open() and other things... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <math.h>
#include <float.h>

#include <unistd.h>		/* close() and other things. */
#include <dirent.h>		/* opendir */

#include <ctype.h>		/* isdigit */

#include "linux/videodev2.h"

#include "CTI.h"
#include "V4L2Capture.h"
#include "Images.h"
#include "String.h"
#include "File.h"
#include "Log.h"
#include "Cfg.h"
#include "Uvc.h"		/* Special code for UVC devices. */
#include "Keycodes.h"
#include "FPS.h"

static void Config_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input V4L2Capture_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_BGR3, OUTPUT_RGB3, OUTPUT_YUV422P, OUTPUT_YUV420P, 
       OUTPUT_JPEG, OUTPUT_O511, OUTPUT_H264, OUTPUT_GRAY };
static Output V4L2Capture_outputs[] = {
  [ OUTPUT_BGR3 ] = { .type_label = "BGR3_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_O511 ] = { .type_label = "O511_buffer", .destination = 0L },
  [ OUTPUT_H264 ] = { .type_label = "H264_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = { .type_label = "GRAY_buffer", .destination = 0L },
};

typedef struct  {
  Instance i;

  /* Many of these variables might end up being strings, and interpreted in set_* functions. */
  String * drivermatch;            /* Optional, I use this for matching gspca sub-devices. */
  String * devpath;
  int fd;
  int enable;			/* Set this to start capturing. */

  String * format;

  /* Mostly relevant for TV/RGB */
  String * input;
  int input_index;
  String * std;
  String * brightness;
  String * saturation;
  String * contrast;

  struct v4l2_frequency freq;
  /* int channel; */  /* There might be more to this, like tuner, etc... */
  /* Capture dimensions. */
  int width;
  int height;

  /* Mostly relevant for JPEG */
  String * autoexpose;
  String * exposure;
  // int auto_expose;
  // int exposure_value;
  int focus;
  int fps;
  float nominal_period;	/* calculated from fps */
  int led1;

  FPS calculated_fps;

  /* Capture buffers.  For many drivers, it is possible to queue more
     than 2 buffers, but I don't see much point.  If the system is too
     busy to schedule the dequeue-ing code, then maybe it is
     underpowered for the application. */
#define NUM_BUFFERS 4
  struct {
    uint8_t *data;
    uint32_t length;
  } buffers[NUM_BUFFERS];
  int wait_on;
  int last_queued;
  int timeout_ms;		/* Frame timeout before full reset */

  int sequence;			/* Keep track of lost frames. */
  int check_sequence;		/* Set to N to check for up to N lost frames. Note: some cards (cx88) don't set sequence! */

  struct v4l2_buffer vbuffer;

  int fix;			/* Fix Jpegs that are missing huffman tables. */

  int snapshot;
  int msg_handled;

  int exit_on_error;

  String * label;

} V4L2Capture_private;


static void get_device_range(Instance *pi, Range *range);
static void stream_disable(V4L2Capture_private *priv);
static int stream_enable(V4L2Capture_private *priv);
static void V4L2_queue_unmap(V4L2Capture_private *priv);

static int set_device(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc = -1;
  struct v4l2_capability v4l2_caps = {};
  struct v4l2_tuner tuner = {};
  int i;

  String_clear(&priv->devpath);

  if (priv->fd != -1) {
    close(priv->fd);
    V4L2_queue_unmap(priv);
    priv->fd = -1;
  }
  
  /* Try matching by description first... */
  Range available_v4l_devices = {};
  get_device_range(pi, &available_v4l_devices);
  for (i=0; i < available_v4l_devices.descriptions.count; i++) {
    printf("strstr( %s, %s )\n",
	   available_v4l_devices.descriptions.items[i]->bytes, value);
    if (strstr(available_v4l_devices.descriptions.items[i]->bytes, value)) {
      puts("found it!");
      String_set(&priv->devpath, available_v4l_devices.strings.items[i]->bytes);
      break;
    }
  }

  Range_clear(&available_v4l_devices);
  
  if (String_is_none(priv->devpath)) {
    /* Not found, try value as a path. */
    String_set(&priv->devpath, value);
  }

  priv->fd = open(s(priv->devpath), O_RDWR);

  if (priv->fd == -1) {
    perror(s(priv->devpath));
    if (priv->exit_on_error) {
      fprintf(stderr, "%s:%s open() failed, exiting\n", __FILE__, __func__);
      exit(1);
    }
    goto out;
  }

  /* This query is also an implicit check for V4L2. */
  rc = ioctl(priv->fd, VIDIOC_QUERYCAP, &v4l2_caps);
  if (-1 == rc) {
    perror("VIDIOC_QUERYCAP");
    close(priv->fd);
    priv->fd = -1;
    goto out;
  }

  if (!String_is_none(priv->drivermatch) && !streq((const char*)v4l2_caps.driver, s(priv->drivermatch))) {
    fprintf(stderr, "driver mismatch %s != %s\n", v4l2_caps.driver, s(priv->drivermatch));
    close(priv->fd);
    priv->fd = -1;
    goto out;
  }

  fprintf(stderr, "card: %s\n", v4l2_caps.card);
  fprintf(stderr, "driver: %s\n", v4l2_caps.driver);

  /* Check tuner capabilities */
  rc = ioctl(priv->fd, VIDIOC_G_TUNER, &tuner);
  if (-1 == rc) {
    /* No big deal.  Maybe warn... */
  }
  else {
    printf("tuner:\n");
    printf("  capability=%x\n", tuner.capability);
    printf("  rangelow=%x\n", tuner.rangelow);
    printf("  rangehigh=%x\n", tuner.rangehigh);
  }

  add_focus_control(priv->fd);
  add_led1_control(priv->fd);

 out:
  return rc;
}


static void get_device_range(Instance *pi, Range *range)
{
  /* Return list of "/dev/video*" and correspoding names from /sys. */

  DIR *d;
  struct dirent *de;
  // V4L2Capture_private *priv = (V4L2Capture_private *)pi;

  range->type = RANGE_STRINGS;

  d = opendir("/dev");

  while (1) {
    de = readdir(d);
    if (!de) {
      closedir(d);
      break;
    }

    if (strncmp(de->d_name, "video", strlen("video")) == 0
	&& isdigit(de->d_name[strlen("video")])) {
      String *s;
      String *desc;

      s = String_new("/dev/");
      String_cat1(s, de->d_name);
      //String_list_append(&r->x.strings.values, &s);

      String *tmp = String_new("");
      String_cat3(tmp, "/sys/class/video4linux/", de->d_name, "/name");
      desc = File_load_text(tmp);
      String_free(&tmp);
      
      String_trim_right(desc);

      printf("%s\n", desc->bytes);

      IndexedSet_add(range->strings, s); s = NULL; /* IndexedSet takes ownership. */
      IndexedSet_add(range->descriptions, desc); desc = NULL; /* IndexedSet takes ownership. */
    }
  }
}

static int set_input(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc;
  int i;
  struct v4l2_input input = {};

  /* Enumerate inputs until a string match is found. */
  i = 0;
  while (1) {
    input.index = i;
    rc = ioctl(priv->fd, VIDIOC_ENUMINPUT, &input);
    if (rc == -1) {
      /* Reached end without finding a match. */
      fprintf(stderr, "input %s not found!\n", value);
      goto out;
    }

    printf("Available input: %s\n", input.name);
    
    if (streq((char*)input.name, value)) {
      printf("Found input: %s\n", input.name);
      priv->input_index = i;
      break;
    }
    i += 1;
  }

  rc = ioctl(priv->fd, VIDIOC_S_INPUT, &input.index);
  if (rc == -1) {
    perror("VIDIOC_S_INPUT");
    goto out;
  }

  /* Everything worked, save value. */
  String_set(&priv->input, value);

 out:
  return rc;
}

#define fourcctostr(fourcc, str) (str[0] = fourcc & 0xff, str[1] = ((fourcc >> 8) & 0xff),  str[2] = ((fourcc >> 16) & 0xff),  str[3] = ((fourcc >> 24) & 0xff), str[4] = 0)

static int set_format(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc;
  int i;
  struct v4l2_format format = {};
  struct v4l2_fmtdesc fmtdesc = {};

  /* Enumerate available pixel formats until a string match is found. */
  i = 0;
  while (1) {
    char fourcc_str[5];
    fmtdesc.index = i;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(priv->fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (rc == -1) {
      /* Reached end without finding a match. */
      fprintf(stderr, "format not found: %s\n", value);
      goto out;
    }

    fourcctostr(fmtdesc.pixelformat, fourcc_str);

    if (streq(fourcc_str, value)) {
      printf("%s: %s\n", fourcc_str, fmtdesc.description);
      break;
    }
    else {
      printf("(%s: %s)\n", fourcc_str, fmtdesc.description);
    }
    i += 1;
  }

  /* Get current format. */
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rc = ioctl(priv->fd, VIDIOC_G_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_G_FMT");
    goto out;
  }

  format.fmt.pix.pixelformat = fmtdesc.pixelformat; /* fmtdesc holds value from loop break */


  /* Set desired format. */
  rc = ioctl(priv->fd, VIDIOC_S_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_S_FMT");
    goto out;
  }

  /* Everything worked, save value. */
  String_set(&priv->format, value);

 out:
  return rc;
}

static void get_format_range(Instance *pi, Range *range)
{
#if 0
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc;
  int i;
  struct v4l2_fmtdesc fmtdesc = {};

  Range *r = Range_new(RANGE_STRINGS);
  
  /* Enumerate pixel formats until a string match is found. */
  i = 0;
  while (1) {
    char fourcc_str[5];
    fmtdesc.index = i;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(priv->fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (rc == -1) {
      /* Reached end. */
      break;
    }

    fourcctostr(fmtdesc.pixelformat, fourcc_str);

    String *tmp = String_new(fourcc_str);
    //String_list_append(&r->x.strings.values, &tmp);

    tmp = String_new((char*)fmtdesc.description);
    //String_list_append(&r->x.strings.descriptions, &tmp);

    i += 1;
  }

  range = r;
#endif
}


static void get_input_range(Instance *pi, Range *range)
{
#if 0
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc;
  int i;
  struct v4l2_input input = {};

  Range *r = Range_new(RANGE_STRINGS);
  
  /* Enumerate inputs. */
  i = 0;
  while (1) {
    input.index = i;
    rc = ioctl(priv->fd, VIDIOC_ENUMINPUT, &input);
    if (rc == -1) {
      /* Reached end. */
      break;
    }

    String *tmp = String_new((char*)input.name);
    //String_list_append(&r->x.strings.values, &tmp);

    tmp = String_new("");

    switch (input.type) {
    case V4L2_INPUT_TYPE_TUNER:  String_cat1(tmp, "tuner"); break;
    case V4L2_INPUT_TYPE_CAMERA: String_cat1(tmp, "camera"); break;
    default: String_cat1(tmp, "unknown type");
    };

    /*
     * NOTE: there are several more fields available, but I'm not handling them yet:
     *   audioset, tuner, std, status
     */

    //String_list_append(&r->x.strings.descriptions, &tmp);

    i += 1;
  }

  *range = r;
#endif
}


static int set_std_priv(V4L2Capture_private *priv)
{
  if (String_is_none(priv->std)) {
    return -1;
  }

  int rc = 0;
  int i;
  struct v4l2_standard standard = {};

  /* Enumerate standards until get a match against a string value... */
  i = 0;
  while (1) {
    standard.index = i;
    rc = ioctl(priv->fd, VIDIOC_ENUMSTD, &standard);
    if (rc == -1) {
      perror("VIDIOC_ENUMSTD");
    goto out;
    }
    
    if (streq((char*)standard.name, s(priv->std)))  {
      printf("%s (%d/%d)\n", standard.name, standard.frameperiod.denominator, standard.frameperiod.numerator);
      break;
    }

    i += 1;
  }

  rc = ioctl(priv->fd, VIDIOC_S_STD, &standard.id);
  if (rc == -1) {
    perror ("VIDIOC_S_STD");
    goto out;
  }

 out:
  return rc;
}

static int set_std(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->std, value);
  return set_std_priv(priv);
}


static int generic_v4l2_set(V4L2Capture_private *priv, uint32_t cid, int value)
{
  int rc = 0;
  struct v4l2_queryctrl queryctrl = {};
  struct v4l2_control control = {};

  queryctrl.id = cid;

  rc = ioctl(priv->fd, VIDIOC_QUERYCTRL, &queryctrl);
  if (rc == -1) {
    perror("VIDIOC_QUERYCTRL");
    goto out;
  }

  control.id = cid;
  control.value = value;

  rc = ioctl(priv->fd, VIDIOC_S_CTRL, &control);
  if (rc == -1) {
    perror("VIDIOC_S_CTRL");
    goto out;
  }

 out:
  return rc;
}

static void generic_v4l2_get_range(V4L2Capture_private *priv, uint32_t cid, const char *label,
				   Range *range)
{
#if 0
  int rc = 0;
  struct v4l2_queryctrl queryctrl = {};

  *range = 0L;
 
  queryctrl.id = cid;

  rc = ioctl(priv->fd, VIDIOC_QUERYCTRL, &queryctrl);
  if (rc == -1) {
    perror("VIDIOC_QUERYCTRL");
    goto out;
  }

  printf("%s: %d..%d step=%d default=%d\n",
	 label,
	 queryctrl.minimum, queryctrl.maximum,
	 queryctrl.step, queryctrl.default_value);

  Range *r = Range_new(RANGE_INTS);
  r->x.ints.min = queryctrl.minimum;
  r->x.ints.max = queryctrl.maximum;
  r->x.ints.step = queryctrl.step;
  r->x.ints._default = queryctrl.default_value;

  *range = r;

 out:
  return;
#endif
}

static void get_brightness_range(Instance *pi, Range *range)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  generic_v4l2_get_range(priv, V4L2_CID_BRIGHTNESS, "brightness", range);
}

static void get_contrast_range(Instance *pi, Range *range)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  generic_v4l2_get_range(priv, V4L2_CID_CONTRAST, "contrast", range);
}

static void get_saturation_range(Instance *pi, Range *range)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  generic_v4l2_get_range(priv, V4L2_CID_SATURATION, "saturation", range);
}


static int set_brightness_priv(V4L2Capture_private *priv)
{
  if (String_is_none(priv->brightness)) {
    return -1;
  }

  int rc = generic_v4l2_set(priv, V4L2_CID_BRIGHTNESS, atoi(s(priv->brightness)));
  if (rc == 0) {
    printf("brightness set to %s\n", s(priv->brightness));
  }
  return rc;
}


static int set_brightness(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->brightness, value);
  return set_brightness_priv(priv);
}

static int set_saturation_priv(V4L2Capture_private *priv)
{
  if (String_is_none(priv->saturation)) {
    return -1;
  }

  int rc;
  rc = generic_v4l2_set(priv, V4L2_CID_SATURATION, atoi(s(priv->saturation)));
  if (rc == 0) {
    String_set(&priv->saturation, s(priv->saturation));
    printf("saturation set to %s\n", s(priv->saturation));
  }
  return rc;
}


static int set_saturation(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->saturation, value);
  return set_saturation_priv(priv);
}



static int set_contrast_priv(V4L2Capture_private *priv)
{
  if (String_is_none(priv->contrast)) {
    return -1;
  }

  int rc = generic_v4l2_set(priv, V4L2_CID_CONTRAST, atoi(s(priv->contrast)));
  if (rc == 0) {
    printf("contrast set to %s\n", s(priv->contrast));
  }
  return rc;
}

static int set_contrast(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->contrast, value);
  return set_contrast_priv(priv);
}

static int set_autoexpose(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;

#ifndef V4L2_CID_EXPOSURE_AUTO
#define V4L2_CID_EXPOSURE_AUTO 10094849
#endif

  rc = generic_v4l2_set(priv, V4L2_CID_EXPOSURE_AUTO, atoi(value));
  if (rc == 0) {
    String_set(&priv->autoexpose, value);
    printf("autoexpose set to %s\n", s(priv->autoexpose));
  }
  return rc;
}


static int set_exposure(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
#ifndef V4L2_CID_EXPOSURE_ABSOLUTE
#define V4L2_CID_EXPOSURE_ABSOLUTE 10094850
#endif
  rc = generic_v4l2_set(priv, V4L2_CID_EXPOSURE_ABSOLUTE, atoi(value));
  if (rc == 0) {
    String_set(&priv->exposure, value);
    printf("exposure set to %s\n", s(priv->exposure));
  }
  return rc;
}


static int set_mute(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  return generic_v4l2_set(priv, V4L2_CID_AUDIO_MUTE, atoi(value));
}

/* begin lucview/v4l2uvc.c sample code */
static int float_to_fraction_recursive(double f, double p, int *num, int *den)
{
	int whole = (int)f;
	f = fabs(f - whole);

	if(f > p) {
		int n, d;
		int a = float_to_fraction_recursive(1 / f, p + p / f, &n, &d);
		*num = d;
		*den = d * a + n;
	}
	else {
		*num = 0;
		*den = 1;
	}
	return whole;
}

static void float_to_fraction(float f, int *num, int *den)
{
	int whole = float_to_fraction_recursive(f, FLT_EPSILON, num, den);
	*num += whole * *den;
}
/* end lucview/v4l2uvc.c sample code */


static int set_fps_priv(V4L2Capture_private *priv)
{
  struct v4l2_streamparm setfps = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
  int rc;
  int n = 0, d = 0;
  int re_enable = 0;

  float_to_fraction(priv->fps, &n, &d);
  setfps.parm.capture.timeperframe.numerator = d;
  setfps.parm.capture.timeperframe.denominator = n;

  if (priv->enable) {
    /* UVC requires stop/restart if capture is running. */
    stream_disable(priv);
    re_enable = 1;
  }

  rc = ioctl(priv->fd, VIDIOC_S_PARM, &setfps);
  if(rc == -1) {
    perror("Unable to set frame rate");
  }
  else {
    rc = ioctl(priv->fd, VIDIOC_G_PARM, &setfps);
    if (rc == 0) {
      printf("frame rate: %d/%d\n", 
	     setfps.parm.capture.timeperframe.numerator,
	     setfps.parm.capture.timeperframe.denominator);
    }
    
  }

  if (re_enable) {
    stream_enable(priv);
  }

  return rc;
}


static int set_fps(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  float fps = atof(value);
  priv->fps = round(fps);
  priv->nominal_period = 1.0/fps;
  return set_fps_priv(priv);
}


static int set_size(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  struct v4l2_format format = {};
  int n, w, h;
  int rc;
  int re_enable = 0;

  n = sscanf(value, "%dx%d", &w, &h);
  if (n != 2) {
    fprintf(stderr, "invalid size string!\n");
    return -1;
  }

  priv->width = w;  
  priv->height = h;

  /* Get current format. */
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rc = ioctl(priv->fd, VIDIOC_G_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_G_FMT");
    goto out;
  }

  /* Set new values; */
  format.fmt.pix.width = priv->width;
  format.fmt.pix.height = priv->height;

  if (priv->enable) {
    re_enable = 1;
    stream_disable(priv);
  }

  rc = ioctl(priv->fd, VIDIOC_S_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_S_FMT");
  }
  else {
    printf("Capture size set to %dx%d\n", format.fmt.pix.width, format.fmt.pix.height);
  }
    
 out:
  if (re_enable) {
    stream_enable(priv);
  }

  if (priv->fps > 0.01) {
    set_fps_priv(priv);
  }

  return rc;
}


static int set_frequency(Instance *pi, const char *value)
{
  /* value should be frequency in Hz. */
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc = -1;
  struct v4l2_frequency freq = {};

  freq.tuner = 0;

  rc = ioctl(priv->fd, VIDIOC_G_FREQUENCY, &freq);
  if (rc == -1) {
    perror("VIDIOC_G_FREQUENCY");
    goto out;
  }

  /* 
   * http://v4l2spec.bytesex.org/spec/r11094.htm: "Tuning frequency in
   * units of 62.5 kHz, or if the struct v4l2_tuner or struct
   * v4l2_modulator capabilities flag V4L2_TUNER_CAP_LOW is set, in
   * units of 62.5 Hz."
   */
  freq.frequency = atol(value)/62500;

#if 1
  rc = ioctl(priv->fd, VIDIOC_S_FREQUENCY, &freq);
  if (rc == -1) {
    perror("VIDIOC_S_FREQUENCY");
    goto out;
  }
#endif

  printf("Tuner frequency set to %d\n", freq.frequency);
 out:
  return rc;
}


static int V4L2_queue_setup(V4L2Capture_private *priv)
{
  int i;
  int rc;

  struct v4l2_requestbuffers req = {
    .count = NUM_BUFFERS,
    .type = priv->vbuffer.type,
    .memory = priv->vbuffer.memory,
  };

  rc = ioctl(priv->fd, VIDIOC_REQBUFS, &req);
  if (rc == -1) {
    perror("VIDIOC_REQBUFS");
    return 1;
  }

  for (i=0; i < req.count; i+=1) {
    priv->vbuffer.index = i;
      
    rc = ioctl(priv->fd, VIDIOC_QUERYBUF, &priv->vbuffer);
    if (-1 == rc) {
      perror("VIDIOC_QUERYBUF");
      return 1;
    }

    priv->buffers[i].data = mmap(0L, priv->vbuffer.length, 
				 PROT_READ | PROT_WRITE, MAP_SHARED, 
				 priv->fd, priv->vbuffer.m.offset);
    if (priv->buffers[i].data == MAP_FAILED) {
      perror("mmap");
      return 1;
    }

    priv->buffers[i].length = priv->vbuffer.length;
  }

  return 0;
}


static void V4L2_queue_unmap(V4L2Capture_private *priv)
{
  int i;
  for (i=0; i < NUM_BUFFERS; i++) {
    if (priv->buffers[i].data) {
      printf("unmapping buffer %d\n", i);
      munmap(priv->buffers[i].data, priv->buffers[i].length);
    }
  }
}


static int stream_enable(V4L2Capture_private *priv)
{
  int i, rc;

  V4L2_queue_setup(priv);

  /* Queue up N-1 buffers.  Have to do this every time the stream is enabled.*/
  for (i=0; i < (NUM_BUFFERS-1); i++) {
    priv->vbuffer.index = i;
    rc = ioctl(priv->fd, VIDIOC_QBUF, &priv->vbuffer);
    if (-1 == rc) {
      perror("VIDIOC_QBUF");
      exit(1);
      return 1;
    }
    priv->last_queued = i;
  }

  int capture = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rc = ioctl(priv->fd, VIDIOC_STREAMON, &capture);
  if (-1 == rc) { 
    perror("VIDIOC_STREAMON"); 
  }

  /* Always start waiting on the first queued buffer, otherwise there
     is a lag NUM_BUFFERS frames. */
  priv->wait_on = 0;		

  return 0;
}


static void stream_disable(V4L2Capture_private *priv)
{
  /* Disable streaming.  According to the docs, "all images captured
     but not dequeued yet will be lost", so the enable function above
     needs to re-queue from scratch. */
  int capture = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int rc = ioctl(priv->fd, VIDIOC_STREAMOFF, &capture);
  if (-1 == rc) { 
    perror("VIDIOC_STREAMOFF"); 
  }
}


static int set_enable(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;

  if (priv->fd == -1) {
    fprintf(stderr, "device is not open!\n");
    priv->enable = 0;
    return 1;
  }

  int newval = atoi(value);
  if (newval == priv->enable) {
    return 1;
  }

  priv->enable = newval;
  printf("V4L2Capture enable set to %d\n", priv->enable);

  if (priv->enable) {
    stream_enable(priv);
  }
  else {
    stream_disable(priv);
  }

  return 0;
}

static int set_snapshot(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  priv->snapshot = atoi(value);
  return 0;
  
}


static int set_fix(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  priv->fix = atoi(value);
  return 0;
  
}


static int set_timeout_ms(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  priv->timeout_ms = atoi(value);
  return 0;
}

static int set_drivermatch(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->drivermatch, value);
  return 0;
}

static int set_label(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  String_set(&priv->label, value);
  return 0;
}

static int set_check_sequence(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  priv->check_sequence = atoi(value);
  return 0;
}

static Config config_table[] = {
  /* NOTE: cti_set_int does not work here.  This module uses a custom
     config handler that sets priv->handled. */
  { "snapshot",   set_snapshot, 0L, 0L},
  { "device",     set_device, 0L, get_device_range},
  { "drivermatch",     set_drivermatch, 0L, 0L},
  { "enable",     set_enable, 0L, 0L },
  { "format",     set_format, 0L, get_format_range},
  { "size",       set_size,   0L, 0L},
  { "input",      set_input,  0L, get_input_range},
  { "std",        set_std,    0L, 0L},
  { "brightness", set_brightness, 0L, get_brightness_range},
  { "saturation", set_saturation, 0L, get_saturation_range},
  { "contrast",   set_contrast, 0L, get_contrast_range},
  { "autoexpose", set_autoexpose, 0L, 0L},
  { "exposure",   set_exposure, 0L, 0L},
  { "mute",       set_mute,   0L, 0L},
  { "fps",        set_fps,   0L, 0L},
  { "frequency",  set_frequency,  0L, 0L},
  { "fix",        set_fix, 0L, 0L}, 
  { "timeout_ms", set_timeout_ms, 0L, 0L},
  { "label",      set_label, 0L, 0L},
  { "check_sequence",   set_check_sequence, 0L, 0L},
};


static int find_control(V4L2Capture_private *priv, const char *label, struct v4l2_queryctrl *qctrl)
{
  int rc;
  int ctrlid;
  int local_verbose = 0;

  { 
    static int pass1 = 1;
    if (pass1) { local_verbose = 1; }
    pass1 = 0;
  }

  printf("[trying standard controls]\n");

  /* Standard controls. */
  for (ctrlid = V4L2_CID_BASE; ctrlid < V4L2_CID_LASTP1; ctrlid++) {
    qctrl->id = ctrlid;
    rc = ioctl(priv->fd, VIDIOC_QUERYCTRL, qctrl);
    if (rc != 0) {
      // fprintf(stderr, "ioctl(VIDIOC_QUERYCTRL, %d) failed\n", qctrl.id);
      // sleep(1);
      continue;
    }

    if (qctrl->flags & V4L2_CTRL_FLAG_DISABLED) {
      /* See "42" in "bttv-driver.c". */
      continue;
    }

    if (streq(label, (const char *)qctrl->name)) {
      return 0;
    }
    else if (local_verbose) {
      fprintf(stderr, "%s\n", qctrl->name);
    }
  }

  printf("[trying private controls]\n");

  /* Driver-specific ("private") controls. */
  for (ctrlid = V4L2_CID_PRIVATE_BASE; ; ctrlid++) {
    qctrl->id = ctrlid;
    rc = ioctl(priv->fd, VIDIOC_QUERYCTRL, qctrl);
    if (rc != 0) {
      /* Not found.  No need to report error. */
      // fprintf(stderr, "ioctl(VIDIOC_QUERYCTRL, %d) failed\n", qctrl->id);
      break;
    }

    printf("%s %s ?\n", label, (const char *)qctrl->name);

    if (qctrl->flags & V4L2_CTRL_FLAG_DISABLED) {
      /* See "42" in "bttv-driver.c". */
      continue;
    }

    if (streq(label, (const char *)qctrl->name)) {
      return 0;
    }
    else if (local_verbose) {
      fprintf(stderr, "%s\n", qctrl->name);
    }
  }

  /* Extended controls, which might be defined by hardware, but not
     known by the driver in advance.   Some controls on the Logitech
     UVC cameras are like this, for example. */
  printf("[trying extended controls]\n");
  qctrl->id = 0;
  while (1) {
    qctrl->id |= V4L2_CTRL_FLAG_NEXT_CTRL; /* Has to be turned on each iteration. */
    rc = ioctl(priv->fd, VIDIOC_QUERYCTRL, qctrl);
    if (rc != 0) {
      // fprintf(stderr, "ioctl(VIDIOC_QUERYCTRL, %d) failed\n", qctrl->id);
      break;
    }

    if (qctrl->flags & V4L2_CTRL_FLAG_DISABLED) {
      /* See "42" in "bttv-driver.c". */
      continue;
    }

    if (streq(label, (const char *)qctrl->name)) {
      return 0;
    }
    else if (local_verbose) {
      fprintf(stderr, "%s\n", qctrl->name);
    }
  }
  

  return -1;
}

/* V4L2 has controls where each has an associated string label.  Search and set... */
static int generic_set(Instance *pi, const char *label, const char *value)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  struct v4l2_queryctrl qctrl = {};
  int i;
  char label_copy[strlen(label+1)];

  for (i=0; i < strlen(label); i++) {
    char c = label[i];
    if (c == '.') {
      label_copy[i] = ' ';
    }
    else {
      label_copy[i] = c;
    }
  }
  label_copy[i] = 0;
  
  fprintf(stderr, "generic_set(%s, %s)\n", label_copy, value);

  if (find_control(priv, label_copy, &qctrl) == 0) {
    int rc;
    struct v4l2_control control = { .id = qctrl.id };

    fprintf(stderr, "%30s(0x%x) %5d-%-5d step %d default:%d", 
	    qctrl.name, qctrl.id, qctrl.minimum, qctrl.maximum, qctrl.step, 
	    qctrl.default_value);

    //ioctl(priv->fd, VIDIOC_G_CTRL, &control);
    //fprintf(stderr, " value: %d\n", control.value);

    if (isdigit(value[0])) {
      int new_value = atoi(value);

      control.value = new_value;
      
      rc = ioctl(priv->fd, VIDIOC_S_CTRL, &control);
      if (rc == -1) {
	perror("VIDIOC_S_CTRL");
      }
    }

    ioctl(priv->fd, VIDIOC_G_CTRL, &control);
    fprintf(stderr, " value: %d\n", control.value);
  }

  return 0;
}


static void Config_handler(Instance *pi, void *data)
{
  int i;
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  Config_buffer *cb_in = data;

  /* First walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      config_table[i].set(pi, cb_in->value->bytes);
      priv->msg_handled = 1;
      break;
    }
  }
  
  /* If not found in config table, try finding the control by label */
  if (!priv->msg_handled) {
    generic_set(pi, cb_in->label->bytes, cb_in->value->bytes);
    priv->msg_handled = 1;
  }
  
  Config_buffer_release(&cb_in);
}

static void Keycode_handler(Instance *pi, void *msg)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  Keycode_message *km = msg;

  if (km->keycode == CTI__KEY_S) {
    priv->snapshot += 1;
  }

  Keycode_message_cleanup(&km);
}


static void jpeg_snapshot(Instance *pi, Jpeg_buffer *j)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  FILE *f;
  char filename[64];

  Jpeg_fix(j);			/* Just in case. */

  sprintf(filename, "snap%06d.jpg", pi->counter);
  fprintf(stderr, "%s\n", filename);
  f = fopen(filename, "wb");
  if (f) {
    if (fwrite(j->data, j->encoded_length, 1, f) != 1) { perror("fwrite"); }
    fclose(f);
  }
  priv->snapshot -= 1;	  
}


static void bgr3_snapshot(Instance *pi, BGR3_buffer *bgr3)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  FILE *f;
  char filename[64];
  sprintf(filename, "snap%04d.ppm", pi->counter);
  f = fopen(filename, "wb");
  if (f) {
    int x, y;
    fprintf(f, "P6\n%d %d\n255\n", bgr3->width, bgr3->height);
    for (y=0; y < bgr3->height; y++) {
      uint8_t *src = bgr3->data + y*bgr3->width*3;
      uint8_t line[bgr3->width*3];
      uint8_t *dst = &line[0];
      for (x=0; x < bgr3->width; x++) {
	dst[0] = src[2];
	dst[1] = src[1];
	dst[2] = src[0];
	dst += 3;
	src += 3;
      }
      if (fwrite(bgr3->data, bgr3->width*3, 1, f) != 1) { perror("fwrite"); }
    }
    fclose(f);
  }
  priv->snapshot -= 1;	  
}


static void yuv420p_snapshot(Instance *pi, YUV420P_buffer *yuv420p)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  FILE *f;
  char filename[64];
  sprintf(filename, "/dev/shm/snap%04d.yuv420p", pi->counter);
  fprintf(stderr, "%s\n", filename);
  f = fopen(filename, "wb");
  if (f) {
    if (fwrite(yuv420p->y, yuv420p->y_length, 1, f) != 1) { perror("fwrite"); }
    if (fwrite(yuv420p->cr, yuv420p->cr_length, 1, f) != 1) { perror("fwrite"); }
    if (fwrite(yuv420p->cb, yuv420p->cb_length, 1, f) != 1) { perror("fwrite"); }
    fclose(f);
  }
  else {
    perror(filename);
  }
  priv->snapshot -= 1;	  
}


static void yuv422p_snapshot(Instance *pi, YUV422P_buffer *y422p)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  FILE *f;
  char filename[64];
  sprintf(filename, "/dev/shm/snap%04d.y422p", pi->counter);
  fprintf(stderr, "%s\n", filename);
  f = fopen(filename, "wb");
  if (f) {
    if (fwrite(y422p->y, y422p->y_length, 1, f) != 1) { perror("fwrite"); }
    if (fwrite(y422p->cr, y422p->cr_length, 1, f) != 1) { perror("fwrite"); }
    if (fwrite(y422p->cb, y422p->cb_length, 1, f) != 1) { perror("fwrite"); }
    fclose(f);
  }
  else {
    perror(filename);
  }
  priv->snapshot -= 1;	  
}


static void V4L2Capture_tick(Instance *pi)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  int rc;
  int wait_flag = (priv->enable ? 0 : 1);
  Handler_message *hm;

  priv->msg_handled = 0;

  hm = GetData(pi, wait_flag);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable) {
    return;
  }

  if (priv->fd == -1) {
    if (!priv->msg_handled) {
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/100}, NULL);
    }
    return;
  }

  /* Queue next frame. */
  priv->vbuffer.index = (priv->last_queued + 1 ) % NUM_BUFFERS;

  rc = ioctl(priv->fd, VIDIOC_QBUF, &priv->vbuffer);
  if (-1 == rc) {
    perror("VIDIOC_QBUF");
    goto out;
  }

  /* Wait on oldest queued. */
  priv->vbuffer.index = priv->wait_on;


  /* Check for timeout.  I wrote this to fix problems with em28xx USB
     capture on aria, which seems to stall under certain conditions, I
     suspect related to lirc_serial calling udelay().  But it might be
     useful in other situations, too. */
  if (priv->timeout_ms) {
    struct pollfd fds[1] = {};
    fds[0].events = POLLIN;
    fds[0].fd = priv->fd;
    rc = poll(fds, 1, priv->timeout_ms);
    if (rc != 1) {
      printf("*** timeout rc=%d, resetting\n", rc);
      close(priv->fd);
      V4L2_queue_unmap(priv);
      priv->fd = open(s(priv->devpath), O_RDWR);
      if (priv->fd == -1) {
	perror(s(priv->devpath));
	goto out;
      }
      else {
	printf("device re-open Ok fd=%d\n", priv->fd);
      }
      
      stream_enable(priv);
      goto out;
    }
  }
  
  rc = ioctl(priv->fd, VIDIOC_DQBUF, &priv->vbuffer);
  if (-1 == rc) {
    perror("VIDIOC_DQBUF");
    sleep(1); 			/* Sleep so don't flood output. */
    goto out;
  }  

  if (priv->check_sequence) {
    int missed = priv->vbuffer.sequence - 1 - priv->sequence;
    if (missed) { 
      printf("%s missed %d frames leading up to %d\n", s(pi->instance_label), missed, priv->vbuffer.sequence);
      if (priv->check_sequence > 0) {
	priv->check_sequence -= 1; /* tick down by 1 */
      }
    }
    priv->sequence = priv->vbuffer.sequence;
  }

  dpf("capture on buffer %d Ok [ priv->format=%s, counter=%d ]\n", priv->wait_on, s(priv->format),
      pi->counter);

  Image_common c = {
    .label = priv->label,
    .nominal_period = priv->nominal_period
  };
  cti_getdoubletime(&c.timestamp);
  FPS_update_timestamp(&priv->calculated_fps, c.timestamp);

  pi->counter += 1;

  /* Check for lack of sync.  I suspect the BTTV would set V4L2_IN_ST_NO_H_LOCK, but cx88 does not,
     so I don't care about it for now. */
  if (0) {
    struct v4l2_input input = { .index = priv->input_index };
    rc = ioctl(priv->fd, VIDIOC_ENUMINPUT, &input);
    if (rc == -1) {
      perror("VIDIOC_ENUMINPUT");
    }
    dpf("status: 0x%08x\n", input.status);
  }

  /* Post to output. */
  if (!s(priv->format)) {
    /* Format not set! */
  }
  else if (streq(s(priv->format), "BGR3")) {
    if (pi->outputs[OUTPUT_BGR3].destination) {
      BGR3_buffer *bgr3 = BGR3_buffer_new(priv->width, priv->height, &c);
      memcpy(bgr3->data, priv->buffers[priv->wait_on].data, priv->width * priv->height * 3);
      if (priv->snapshot > 0) {
	bgr3_snapshot(pi, bgr3);
      }
      PostData(bgr3, pi->outputs[OUTPUT_BGR3].destination);
    }
    if (pi->outputs[OUTPUT_RGB3].destination) {
      RGB3_buffer *rgb3 = 0L;
      BGR3_buffer *bgr3 = BGR3_buffer_new(priv->width, priv->height, &c);
      memcpy(bgr3->data, priv->buffers[priv->wait_on].data, priv->width * priv->height * 3);
      if (priv->snapshot > 0) {
	bgr3_snapshot(pi, bgr3);
      }
      bgr3_to_rgb3(&bgr3, &rgb3);
      PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
    }
  }
  else if (streq(s(priv->format), "422P")) {
    if (pi->outputs[OUTPUT_YUV422P].destination) {
      YUV422P_buffer *y422p = YUV422P_buffer_new(priv->width, priv->height, &c);
      Log(LOG_YUV422P, "%s allocated y422p @ %p", __func__, y422p);
      memcpy(y422p->y, priv->buffers[priv->wait_on].data + 0, priv->width*priv->height);
      memcpy(y422p->cb, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height, 
	     priv->width*priv->height/2);
      memcpy(y422p->cr, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height + priv->width*priv->height/2,
	     priv->width*priv->height/2);
      Log(LOG_YUV422P, "%s posting y422p @ %p", __func__, y422p);
      if (priv->snapshot > 0) {
	yuv422p_snapshot(pi, y422p);
      }
      PostData(y422p, pi->outputs[OUTPUT_YUV422P].destination);
    }

    if (pi->outputs[OUTPUT_GRAY].destination) {
      Gray_buffer *g = Gray_buffer_new(priv->width, priv->height, &c);
      memcpy(g->data, priv->buffers[priv->wait_on].data + 0, priv->width*priv->height);      
      PostData(g, pi->outputs[OUTPUT_GRAY].destination);
    }
  }
  else if (streq(s(priv->format), "YU12")) {
    if (pi->outputs[OUTPUT_YUV420P].destination) {
      YUV420P_buffer *y420p = YUV420P_buffer_new(priv->width, priv->height, &c);
      dpf("%s allocated y420p @ %p", __func__, y420p);
      memcpy(y420p->y, priv->buffers[priv->wait_on].data + 0, priv->width*priv->height);
      memcpy(y420p->cb, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height, 
	     y420p->cr_width*y420p->cr_height);
      memcpy(y420p->cr, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height + y420p->cr_width*y420p->cr_height,
	     y420p->cb_width*y420p->cb_height);
      if (priv->snapshot > 0) {
	yuv420p_snapshot(pi, y420p);
      }
      dpf("%s posting y420p @ %p", __func__, y420p);
      PostData(y420p, pi->outputs[OUTPUT_YUV420P].destination);
    }

    if (pi->outputs[OUTPUT_GRAY].destination) {
      Gray_buffer *g = Gray_buffer_new(priv->width, priv->height, &c);
      memcpy(g->data, priv->buffers[priv->wait_on].data + 0, priv->width*priv->height);      
      PostData(g, pi->outputs[OUTPUT_GRAY].destination);
    }
  }
  else if (streq(s(priv->format), "YUYV")) {
    if (priv->vbuffer.bytesused != priv->width*priv->height*2) {
      fprintf(stderr, "%s: YUYV buffer is not the expected size!\n", __func__);
    }
    if (pi->outputs[OUTPUT_YUV422P].destination) {
      int i;
      int iy = 0;
      int icr = 0;
      int icb = 0;
      uint8_t *p = priv->buffers[priv->wait_on].data;
      YUV422P_buffer *y422p = YUV422P_buffer_new(priv->width, priv->height, &c);
      for (i=0; i < priv->vbuffer.bytesused/4; i++) {
	/* YUYV is packed-pixels, need to sort to planes for YUV422p.
	 * Current SSE versions don't support scatter/gather, so
	 * there's no easy way to acclerate this.  Well, I could try
	 * doing it in 3 passes using SSE shuffle instructions. */
	y422p->y[iy++] = *p++;
	y422p->cb[icb++] = *p++;
	y422p->y[iy++] = *p++;
	y422p->cr[icr++] = *p++;
      }
      if (priv->snapshot > 0) {
	yuv422p_snapshot(pi, y422p);
      }
      PostData(y422p, pi->outputs[OUTPUT_YUV422P].destination);
    }

    if (pi->outputs[OUTPUT_GRAY].destination) {
      int i = 0;
      Gray_buffer *g = Gray_buffer_new(priv->width, priv->height, &c);
      /* Every 2nd pixel will be a gray value. */
      for (i=0; i < priv->vbuffer.bytesused/2; i++) {
	g->data[i] = priv->buffers[priv->wait_on].data[i*2];
      }
      PostData(g, pi->outputs[OUTPUT_GRAY].destination);
    }
  }
  else if (streq(s(priv->format), "JPEG") ||
	   streq(s(priv->format), "MJPG") ) {
    if (pi->outputs[OUTPUT_JPEG].destination) {
      Jpeg_buffer *j = Jpeg_buffer_new(priv->vbuffer.bytesused, &c);
      /* Had been setting this for VirtualStorage,
  	   j->c.label = String_new("/snapshot.jpg") 
	 but that should be done via config now. */
      memcpy(j->data, priv->buffers[priv->wait_on].data, priv->vbuffer.bytesused);
      j->encoded_length = priv->vbuffer.bytesused;
      if (priv->fix) { Jpeg_fix(j); }
      if (j->encoded_length < 1000) {
	printf("encoded_length=%lu, probably isochronous error!\n", j->encoded_length);
	Jpeg_buffer_release(j);
      }
      else {
	if (priv->snapshot > 0) {
	  jpeg_snapshot(pi, j);
	}
	PostData(j, pi->outputs[OUTPUT_JPEG].destination);
      }
    }
  }
  else if (streq(s(priv->format), "O511")) {
    if (pi->outputs[OUTPUT_O511].destination) {
      O511_buffer *o = O511_buffer_new(priv->width, priv->height, &c);
      memcpy(o->data, priv->buffers[priv->wait_on].data, priv->vbuffer.bytesused);

      dpf("O511 bytesused=%d\n", priv->vbuffer.bytesused);

      o->encoded_length = priv->vbuffer.bytesused;

      if (o->encoded_length < 1000) {
	printf("encoded_length=%lu, probably isochronous error!\n", o->encoded_length);
	O511_buffer_release(o);
      }
      else {
	PostData(o, pi->outputs[OUTPUT_O511].destination);
      }
    }
  }
  else {
    fprintf(stderr, "%s: format %s not handled!\n", __func__, s(priv->format));
  }

  /* Update queue indexes. */
  priv->last_queued = (priv->last_queued + 1 ) % NUM_BUFFERS;
  priv->wait_on = (priv->wait_on + 1 ) % NUM_BUFFERS;

 out:
  return;
}


static void V4L2Capture_instance_init(Instance *pi)
{
  V4L2Capture_private *priv = (V4L2Capture_private *)pi;
  priv->fd = -1;
  priv->check_sequence = 0;
  priv->vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  priv->vbuffer.memory = V4L2_MEMORY_MMAP;  /* Change this later if device doesn't support mmap */

  priv->drivermatch = String_value_none();
  priv->devpath = String_value_none();
  priv->format = String_value_none();
  priv->input = String_value_none();
  priv->std = String_value_none();
  priv->brightness = String_value_none();
  priv->saturation = String_value_none();
  priv->contrast = String_value_none();
  priv->autoexpose = String_value_none();
  priv->exposure = String_value_none();

  priv->label = String_value_none();

  priv->exit_on_error = 1;
}

static Template V4L2Capture_template = {
  .label = "V4L2Capture",
  .priv_size = sizeof(V4L2Capture_private),
  .inputs = V4L2Capture_inputs,
  .num_inputs = table_size(V4L2Capture_inputs),
  .outputs = V4L2Capture_outputs,
  .num_outputs = table_size(V4L2Capture_outputs),
  .tick = V4L2Capture_tick,
  .instance_init = V4L2Capture_instance_init,
};

void V4L2Capture_init(void)
{
  Template_register(&V4L2Capture_template);
}

