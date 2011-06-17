/* 
 * V4L2 capture.  I would like this work for BTTV (RGB, YCrCb) and
 * Logitech (MJPEG, YUV).  Ooo, it also works for Topro!  Well, sorta.
 * When the Topro feels like working.
 */

#include <stdio.h>
#include <string.h>
// extern char *strdup(const char *s); /* why not getting this from string.h??  Oh, -std=c99... */
#define streq(a, b)  (strcmp(a, b) == 0)
#include <stdlib.h>
#include <sys/ioctl.h>		/* ioctl */
#include <sys/mman.h>		/* mmap */

/* open() an other things... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

static void Config_handler(Instance *pi, void *data);

enum { INPUT_CONFIG };
static Input V4L2Capture_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_BGR3, OUTPUT_RGB3, OUTPUT_422P, OUTPUT_JPEG, OUTPUT_O511, OUTPUT_H264 };
static Output V4L2Capture_outputs[] = {
  [ OUTPUT_BGR3 ] = { .type_label = "BGR3_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_422P ] = { .type_label = "422P_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
  [ OUTPUT_O511 ] = { .type_label = "O511_buffer", .destination = 0L },
  [ OUTPUT_H264 ] = { .type_label = "H264_buffer", .destination = 0L },
};

typedef struct  {
  /* Many of these variables might end up being strings, and interpreted in set_* functions. */
  char *devpath;
  int fd;
  int enable;			/* Set this to start capturing. */

  char *format;

  /* Mostly relevant for TV/RGB */
  char *input;
  int input_index;
  char *std;
  char *brightness;
  char *saturation;
  char *contrast;

  struct v4l2_frequency freq;
  /* int channel; */  /* There might be more to this, like tuner, etc... */
  int width;
  int height;

  /* Mostly relevant for JPEG */
  char *autoexpose;
  char *exposure;
  // int auto_expose;
  // int exposure_value;
  int focus;
  int fps;
  int led1;

  /* Capture buffers.  For many drivers, it is possible to queue more
     than 2 buffers, but I don't see much point.  If the system is too
     busy to schedule the dequeue-ing code, then maybe it is
     underpowered for the application.  Well, I'll use 4... */
#define NUM_BUFFERS 4
  struct {
    uint8_t *data;
    uint32_t length;
  } buffers[NUM_BUFFERS];
  int wait_on;
  int last_queued;

  int sequence;			/* Keep track of lost frames. */
  int check_sequence;		/* Some cards (cx88) don't set sequence! */

  struct v4l2_buffer vbuffer;

  int snapshot;
  int msg_handled;

} V4L2Capture_private;


static int set_device(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  int rc = -1;
  struct v4l2_capability v4l2_caps = {};
  struct v4l2_tuner tuner = {};

  if (priv->devpath) {
    free(priv->devpath);
  }
  priv->devpath = strdup(value);

  if (priv->fd != -1) {
    close(priv->fd);
    priv->fd = -1;
  }

  priv->fd = open(priv->devpath, O_RDWR);

  if (priv->fd == -1) {
    /* FIXME: Set error status, do not call perror. */
    perror(priv->devpath);
    goto out;
  }

  /* This query is also an implicit check for V4L2. */
  rc = ioctl(priv->fd, VIDIOC_QUERYCAP, &v4l2_caps);
  if (-1 == rc) {
    /* FIXME: Set error status, do not call perror. */
    perror(priv->devpath);
    close(priv->fd);
    priv->fd = -1;
    goto out;
  }
  /* FIXME: Store ".driver" field from result. */
  fprintf(stderr, "card: %s\n", v4l2_caps.card);


  /* Check tuner capabilities */
  rc = ioctl(priv->fd, VIDIOC_G_TUNER, &tuner);
  if (-1 == rc) {
    perror(priv->devpath);
  }
  else {
    printf("tuner:\n");
    printf("  capability=%x\n", tuner.capability);
    printf("  rangelow=%x\n", tuner.rangelow);
    printf("  rangehigh=%x\n", tuner.rangehigh);
  }

  fprintf(stderr, "card: %s\n", v4l2_caps.card);

  add_focus_control(priv->fd);
  add_led1_control(priv->fd);

 out:
  return rc;
}


static void get_device_range(Instance *pi, Range *range)
{
#if 0
  /* Return list of "/dev/video*" and correspoding names from /sys. */

  DIR *d;
  struct dirent *de;
  // V4L2Capture_private *priv = pi->data;

  Range *r = Range_new(RANGE_STRINGS);

  d = opendir("/dev");

  while (1) {
    de = readdir(d);
    if (!de) {
      break;
    }

    if (strncmp(de->d_name, "video", strlen("video")) == 0
	&& isdigit(de->d_name[strlen("video")])) {
      String *s;

      s = String_new("/dev/");
      String_cat1(s, de->d_name);
      //String_list_append(&r->x.strings.values, &s);

      s = String_new("");
      String_cat3(s, "/sys/class/video4linux/", de->d_name, "/name");

      String *desc = File_load_text(s->bytes);
      String_free(&s);
      
      String_trim_right(desc);

      //String_list_append(&r->x.strings.descriptions, &desc);
    }
  }

  *range = r;
#endif

}

static int set_input(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
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
  if (priv->input) {
    free(priv->input);
  }
  priv->input = strdup(value);

 out:
  return rc;
}

#define fourcctostr(fourcc, str) (str[0] = fourcc & 0xff, str[1] = ((fourcc >> 8) & 0xff),  str[2] = ((fourcc >> 16) & 0xff),  str[3] = ((fourcc >> 24) & 0xff), str[4] = 0)

static int set_format(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  int rc;
  int i;
  struct v4l2_format format = {};
  struct v4l2_fmtdesc fmtdesc = {};

  /* Enumerate pixel formats until a string match is found. */
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

  rc = ioctl(priv->fd, VIDIOC_S_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_S_FMT");
    goto out;
  }

  /* Everything worked, save value. */
  if (priv->format) {
    free(priv->format);
  }
  priv->format = strdup(value);

 out:
  return rc;
}

static void get_format_range(Instance *pi, Range *range)
{
#if 0
  V4L2Capture_private *priv = pi->data;
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
  V4L2Capture_private *priv = pi->data;
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


static int set_std(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
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
    
    if (streq((char*)standard.name, value))  {
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

  /* Everything worked, save value. */
  if (priv->std) {
    free(priv->std);
  }
  priv->std = strdup(value);

 out:
  return rc;
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
  V4L2Capture_private *priv = pi->data;
  generic_v4l2_get_range(priv, V4L2_CID_BRIGHTNESS, "brightness", range);
}

static void get_contrast_range(Instance *pi, Range *range)
{
  V4L2Capture_private *priv = pi->data;
  generic_v4l2_get_range(priv, V4L2_CID_CONTRAST, "contrast", range);
}

static void get_saturation_range(Instance *pi, Range *range)
{
  V4L2Capture_private *priv = pi->data;
  generic_v4l2_get_range(priv, V4L2_CID_SATURATION, "saturation", range);
}


static int set_brightness(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = pi->data;
  rc = generic_v4l2_set(priv, V4L2_CID_BRIGHTNESS, atoi(value));
  if (rc == 0) {
    if (priv->brightness) {
      free(priv->brightness);
    }
    priv->brightness = strdup(value);
    printf("brightness set to %s\n", priv->brightness);
  }
  return rc;
}

static int set_contrast(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = pi->data;
  rc = generic_v4l2_set(priv, V4L2_CID_CONTRAST, atoi(value));
  if (rc == 0) {
    if (priv->contrast) {
      free(priv->contrast);
    }
    priv->contrast = strdup(value);
    printf("contrast set to %s\n", priv->contrast);
  }
  return rc;
}

static int set_autoexpose(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = pi->data;

#ifndef V4L2_CID_EXPOSURE_AUTO
#define V4L2_CID_EXPOSURE_AUTO 10094849
#endif

  rc = generic_v4l2_set(priv, V4L2_CID_EXPOSURE_AUTO, atoi(value));
  if (rc == 0) {
    if (priv->autoexpose) {
      free(priv->autoexpose);
    }
    priv->autoexpose = strdup(value);
    printf("autoexpose set to %s\n", priv->autoexpose);
  }
  return rc;
}


static int set_exposure(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = pi->data;
#ifndef V4L2_CID_EXPOSURE_ABSOLUTE
#define V4L2_CID_EXPOSURE_ABSOLUTE 10094850
#endif
  rc = generic_v4l2_set(priv, V4L2_CID_EXPOSURE_ABSOLUTE, atoi(value));
  if (rc == 0) {
    if (priv->exposure) {
      free(priv->exposure);
    }
    priv->exposure = strdup(value);
    printf("exposure set to %s\n", priv->exposure);
  }
  return rc;
}

static int set_saturation(Instance *pi, const char *value)
{
  int rc;
  V4L2Capture_private *priv = pi->data;
  rc = generic_v4l2_set(priv, V4L2_CID_SATURATION, atoi(value));
  if (rc == 0) {
    if (priv->saturation) {
      free(priv->saturation);
    }
    priv->saturation = strdup(value);
    printf("saturation set to %s\n", priv->saturation);
  }
  return rc;
}

static int set_mute(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  return generic_v4l2_set(priv, V4L2_CID_AUDIO_MUTE, atoi(value));
}

/* begin lucview/v4l2uvc.c sample code */
#include <math.h>
#include <float.h>
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


static int set_fps(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  struct v4l2_streamparm setfps = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
  int rc;
  int n = 0, d = 0;
  float fps = atof(value);

  /* FIXME: Should stop/restart the video for UVC. */

  float_to_fraction(fps, &n, &d);
  setfps.parm.capture.timeperframe.numerator = d;
  setfps.parm.capture.timeperframe.denominator = n;

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
  return rc;
}

static int set_size(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  struct v4l2_format format = {};
  int n, w, h;
  int rc;

  /* FIXME: For certain capture devices, like UVC, if currently
     running, need to stop and drain frames, then restart after
     resizing. */

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

  rc = ioctl(priv->fd, VIDIOC_S_FMT, &format);
  if (rc == -1) {
    perror("VIDIOC_S_FMT");
    goto out;
  }

  printf("Capture size set to %dx%d\n", format.fmt.pix.width, format.fmt.pix.height);
    
 out:
  return rc;
  
}


static int set_frequency(Instance *pi, const char *value)
{
  /* value should be frequency in Hz. */
  V4L2Capture_private *priv = pi->data;
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


static int set_enable(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  if (priv->fd == -1) {
    fprintf(stderr, "device is not open!\n");
    priv->enable = 0;
  }
  else {
    priv->enable = atoi(value);
  }
  
  printf("V4L2Capture enable set to %d\n", priv->enable);

  return 0;
}

static int set_snapshot(Instance *pi, const char *value)
{
  V4L2Capture_private *priv = pi->data;
  priv->snapshot = atoi(value);
  return 0;
  
}


static Config config_table[] = {
  { "snapshot",   set_snapshot, 0L, 0L},
  { "device",     set_device, 0L, get_device_range},
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

  puts(__func__);

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
  V4L2Capture_private *priv = pi->data;
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

  /* Queue up N-1 buffers. */
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

  /* Enable streaming. */
  int capture = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rc = ioctl(priv->fd, VIDIOC_STREAMON, &capture);
  if (-1 == rc) { 
    perror("VIDIOC_STREAMON"); 
    /* continue despite error... */
  }

  return 0;
}


static void Config_handler(Instance *pi, void *data)
{
  int i;
  V4L2Capture_private *priv = pi->data;
  Config_buffer *cb_in = data;

  /* First walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      priv->msg_handled = 1;
      break;
    }
  }
  
  /* If not found in config table, try finding the control by label */
  if (!priv->msg_handled) {
    generic_set(pi, cb_in->label->bytes, cb_in->value->bytes);
    priv->msg_handled = 1;
  }
  
  Config_buffer_discard(&cb_in);
}

static double period = 0.0;
static int count = 0;
static struct timeval tv_last = { };
static void calc_fps(struct timeval *tv)
{
  double t1, t2, p;

  if (!tv_last.tv_sec) {
    goto out;
  }

  t1 = tv_last.tv_sec + (tv_last.tv_usec/1000000.0);
  t2 = tv->tv_sec + (tv->tv_usec/1000000.0);
  p = t2 - t1;

  if (period == 0.0) {
    period = p;
  }
  else {
    period = ((period * 19) + (p))/20;
    if (++count % 10 == 0 && cfg.verbosity) {
      printf("p=%.3f %.3f fps\n", p, (1.0/period));
    }
  }

 out:
  tv_last = *tv;
}


static void V4L2Capture_tick(Instance *pi)
{
  V4L2Capture_private *priv = pi->data;
  int rc;
  int wait_flag = (priv->enable ? 0 : 1);
  Handler_message *hm;

  priv->msg_handled = 0;

  hm = GetData(pi, wait_flag);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
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

  /* If no frames queued, set things up and queue a first frame. */
  if (priv->buffers[0].data == 0L) {
    V4L2_queue_setup(priv);
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
  rc = ioctl(priv->fd, VIDIOC_DQBUF, &priv->vbuffer);
  if (-1 == rc) {
    perror("VIDIOC_DQBUF");
    /* FIXME: How to handle dequeue errors?  Probably easiest to just shut
       things down and invalidate the object. */
    exit(1);
    goto out;
  }  

  if (priv->check_sequence) {
    int missed = priv->vbuffer.sequence - 1 - priv->sequence;
    if (missed) { 
      printf("missed %d frames leading up to %d\n", missed, priv->vbuffer.sequence);
      if (priv->vbuffer.sequence == 0) {
	/* cx88 does not set buffer.sequence, so stop checking. */
	priv->check_sequence = 0;
      }
    }
    priv->sequence = priv->vbuffer.sequence;
  }

  if (cfg.verbosity >= 2) {
    printf("capture on buffer %d Ok [ priv->format=%s, counter=%d ]\n", priv->wait_on, priv->format,
	   pi->counter);
  }
  pi->counter += 1;

  /* Check for lack of sync.  I suspect the BTTV would set V4L2_IN_ST_NO_H_LOCK, but cx88 does not,
     so I don't care about it for now. */
  if (0) {
    struct v4l2_input input = { .index = priv->input_index };
    rc = ioctl(priv->fd, VIDIOC_ENUMINPUT, &input);
    if (rc == -1) {
      perror("VIDIOC_ENUMINPUT");
    }
    else if (cfg.verbosity >= 2) {
      printf("status: 0x%08x\n", input.status);
    }
  }
  

  /* Process if needed.  Maybe keep minimal, do processing in other Instances...*/

  /* Post to output. */
  if (!priv->format) {
    /* Format not set! */
  }
  else if (streq(priv->format, "BGR3")) {
    BGR3_buffer *bgr3 = BGR3_buffer_new(priv->width, priv->height);
    memcpy(bgr3->data, priv->buffers[priv->wait_on].data, priv->width * priv->height * 3);
    if (pi->outputs[OUTPUT_BGR3].destination) {
      PostData(bgr3, pi->outputs[OUTPUT_BGR3].destination);
    }
    else if (pi->outputs[OUTPUT_RGB3].destination) {
      RGB3_buffer *rgb3 = 0L;
      bgr3_to_rgb3(&bgr3, &rgb3);
      PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
    }
  }
  else if (streq(priv->format, "422P")) {
    if (pi->outputs[OUTPUT_422P].destination) {
      Y422P_buffer *y422p = Y422P_buffer_new(priv->width, priv->height);
      Log(LOG_Y422P, "%s allocated y422p @ %p", __func__, y422p);
      gettimeofday(&y422p->tv, NULL);
      memcpy(y422p->y, priv->buffers[priv->wait_on].data + 0, priv->width*priv->height);
      memcpy(y422p->cb, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height, 
	     priv->width*priv->height/2);
      memcpy(y422p->cr, 
	     priv->buffers[priv->wait_on].data + priv->width*priv->height + priv->width*priv->height/2,
	     priv->width*priv->height/2);
      Log(LOG_Y422P, "%s posting y422p @ %p", __func__, y422p);
      PostData(y422p, pi->outputs[OUTPUT_422P].destination);
    }
  }
  else if (streq(priv->format, "JPEG") ||
	   streq(priv->format, "MJPG") ) {
    if (pi->outputs[OUTPUT_JPEG].destination) {
      Jpeg_buffer *j = Jpeg_buffer_new(priv->vbuffer.bytesused);
      gettimeofday(&j->tv, NULL);
      calc_fps(&j->tv);
      memcpy(j->data, priv->buffers[priv->wait_on].data, priv->vbuffer.bytesused);
      j->encoded_length = priv->vbuffer.bytesused;
      if (j->encoded_length < 1000) {
	printf("encoded_length=%d, probably isochronous error!\n", j->encoded_length);
	Jpeg_buffer_discard(j);
      }
      else {
	if (priv->snapshot > 0) {
	  FILE *f;
	  char filename[64];
	  sprintf(filename, "snap%04d.jpg", pi->counter);
	  f = fopen(filename, "wb");
	  if (f) {
	    if (fwrite(j->data, j->encoded_length, 1, f) != 1) { perror("fwrite"); }
	    fclose(f);
	  }
	  priv->snapshot -= 1;	  
	}
	PostData(j, pi->outputs[OUTPUT_JPEG].destination);
      }
    }
  }
  else if (streq(priv->format, "O511")) {
    if (pi->outputs[OUTPUT_O511].destination) {
      O511_buffer *o = O511_buffer_new(priv->width, priv->height);
      gettimeofday(&o->tv, NULL);
      memcpy(o->data, priv->buffers[priv->wait_on].data, priv->vbuffer.bytesused);

      o->encoded_length = priv->vbuffer.bytesused;
      if (o->encoded_length < 1000) {
	printf("encoded_length=%d, probably isochronous error!\n", o->encoded_length);
	O511_buffer_discard(o);
      }
      else {
	PostData(o, pi->outputs[OUTPUT_O511].destination);
      }
    }
  }
  else {
    fprintf(stderr, "%s: format %s not handled!\n", __func__, priv->format);
  }

  /* Update queue indexes. */
  priv->last_queued = (priv->last_queued + 1 ) % NUM_BUFFERS;
  priv->wait_on = (priv->wait_on + 1 ) % NUM_BUFFERS;

 out:
  return;
}


static void V4L2Capture_instance_init(Instance *pi)
{
  V4L2Capture_private *priv = Mem_calloc(1, sizeof(*priv));
  priv->fd = -1;
  priv->check_sequence = 1;
  priv->vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  priv->vbuffer.memory = V4L2_MEMORY_MMAP;  /* Change this later if device doesn't support mmap */
  pi->data = priv;
}

static Template V4L2Capture_template = {
  .label = "V4L2Capture",
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

