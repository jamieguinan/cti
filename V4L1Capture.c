/* V4L (old API, "1") code.  This is obsolete, but works great for
   OV511 cameras attached to LPD boards, and I have no plans to update
   that platform to kernels beyond 2.6.x */

#include <sys/ioctl.h>
#include <string.h>		/* memcpy */
#include <stdio.h>
#include <stdlib.h>		/* exit */
#include <sys/mman.h>		/* mmap */

#include "V4L1Capture.h"
#include "Mem.h"
#include "Array.h"

#define table_size(x) (sizeof(x)/sizeof(x[0]))

#define rc_check(x)  { if (rc == -1) { perror(x); exit(1); }  }

#include <sys/time.h>           /* Needed for "struct timeval" member in videodev2.h */
#include <linux/videodev.h>     /* May includes videodev2.h */


/* vvvvvvvvvvvvv */
/* Temporary code from old Vid.h */
typedef struct _Vid_instance Vid_instance;

typedef struct {
  int (*set_w_h)(Vid_instance *v, int w, int h);
  int (*set_brightness)(Vid_instance *v, int brightness);
  int (*set_exposure)(Vid_instance *v, int exposure);
  int (*get_frame)(Vid_instance *v);
} _Vid_instance_ops;


typedef struct {
  uint32_t fourcc;		/* Matching V4L2 */
  const char *label;
  int weight;			/* Preference factor. */
} VidFormat;

struct _Vid_instance {
  int fd;
  int w, h;
  int brightness;		/* Synched from lower layer. */
  int exposure;			/* Synched from lower layer. */
  const VidFormat *format;
  ArrayU8 *frame;		/* Most recent captured frame. */
  void *data;	                /* interface-specific data */
  _Vid_instance_ops *ops;	/* interface-specific operations */
};



typedef struct {
} _Vid_interface;

/* ^^^^^^^^^^^^ */

typedef struct {
  struct video_channel channel;
  struct video_tuner *tuners;
} ChannelWithTuners;

typedef struct {
  struct video_capability cap;  
  ChannelWithTuners *vchannels; 
  struct video_audio *vaudios;
  struct video_mbuf vmbuf;	/* Info about mmap-able area. */
  struct video_mmap vmmap;	/* mmap configuration via ioctl */
  uint8_t *map;			/* mapped memory */
  struct video_picture picture;	/* picture, erm, parameters */

  struct video_window window;	/* needed for setting output size correctly with ov511? */

  struct video_capture capture;	/* For changing capture size. */

  int next_frame;

  // int current_width;  /* superceded by Vid_instance fields */
  // int current_height;

#if 0
  Frame *frames;
  GString *channel_str;
  GString *next_channel_str;
  int channel_number;
#endif

  int status;			/* Current status. */
} V4L1_instance;

static int V4L1_set_w_h(Vid_instance *v, int w, int h)
{
  V4L1_instance *v1 = v->data;
  int rc;

  puts("V4L1_set_w_h");

  v->w = w;
  v->h = h;

  /* VIDIOCGSWIN isn't needed by some drivers (ov511), but others
     require it (zoran), so I always call it. */
  rc = ioctl(v->fd, VIDIOCGWIN, &v1->window);
  rc_check("VIDIOCGWIN");
  v1->window.width = v->w;
  v1->window.height = v->h;
  v1->window.clips = NULL;
  v1->window.clipcount = 0;
  while (1) {
    rc = ioctl(v->fd, VIDIOCSWIN, &v1->window);
    // rc_check("VIDIOCSWIN");
    if (rc == 0) { 
      break;
    }
    perror("VIDIOCSWIN");
  }
  printf("window: %dx%d @(%d,%d)\n", v1->window.width, v1->window.height,
	 v1->window.x, v1->window.y);

  /* ... */
  rc = ioctl(v->fd, VIDIOCGPICT, &v1->picture);
  rc_check("VIDIOCGPICT");

  /* For CMCAPTURE */
  v1->vmmap.width = v->w;
  v1->vmmap.height = v->h;

  return 0;
}

static int V4L1_set_brightness(Vid_instance *v, int brightness)
{
  int rc;
  V4L1_instance *v1 = v->data;

  v->brightness = v1->picture.brightness = brightness;
  rc = ioctl(v->fd, VIDIOCSPICT, &v1->picture);
  printf("rc=%d\n", rc);
  rc_check("VIDIOCSPICT");

  return 0;
}

static int V4L1_set_exposure(Vid_instance *v, int exposure)
{
  int rc;
  V4L1_instance *v1 = v->data;

  v->exposure = v1->picture.whiteness = exposure;
  rc = ioctl(v->fd, VIDIOCSPICT, &v1->picture);
  printf("rc=%d\n", rc);
  rc_check("VIDIOCSPICT");

  return 0;
}

static int V4L1_get_frame(Vid_instance *v)
{
  int rc;
  int current_frame;

  // puts(__func__);

  V4L1_instance *v1 = v->data;

  current_frame = v1->next_frame;

  /* If more than 1 frame, can queue up next capture before waiting for last one to complete. */
  printf("%d frames\n", v1->vmbuf.frames);
  if (v1->vmbuf.frames > 1) {
    v1->next_frame = (v1->next_frame + 1) % v1->vmbuf.frames;
    v1->vmmap.frame = v1->next_frame;
    /* printf("capturing %dx%d\n",v1->vmmap.width, v1->vmmap.height); */
    rc = ioctl(v->fd, VIDIOCMCAPTURE, &v1->vmmap /* .frame */);
    rc_check("VIDIOCMCAPTURE");
    
  }

  rc = ioctl(v->fd, VIDIOCSYNC, &current_frame);
  rc_check("VIDIOCSYNC");
  
  /* If only a single frame, queue the next capture now. */
  if (v1->vmbuf.frames == 1) {
    rc = ioctl(v->fd, VIDIOCMCAPTURE, &v1->vmmap /* .frame */);  
  }

  /* Use depth to calculate captured frame size. */
  int img_size = (v->w * v->h * v1->picture.depth)/8;

  ArrayU8 *img = ArrayU8_new();
  ArrayU8_append(img, 
		 ArrayU8_temp_const(v1->map + v1->vmbuf.offsets[current_frame], img_size));

  if (v->frame) {
    // FIXME:  If doing mark/sweep, can leave this float.  Otherwise
    // should unref previous frame.
  }
  v->frame = img;
  
  return 0;
}

_Vid_instance_ops V4L1_instance_ops = {
  .set_w_h = V4L1_set_w_h,
  .set_brightness = V4L1_set_brightness,
  .set_exposure = V4L1_set_exposure,
  .get_frame = V4L1_get_frame,
};

static void V4L1_init(Vid_instance *v)
{
  int rc;
  int i, j;

  V4L1_instance *v1 = Mem_malloc(sizeof(V4L1_instance));
  v->data = v1;

  rc = ioctl(v->fd, VIDIOCGCAP, &v1->cap);
  printf("rc=%d\n", rc);
  rc_check("VIDIOCGCCAP");

  printf("name: %s, max size: %dx%d\n", v1->cap.name, v1->cap.maxwidth, v1->cap.maxheight);

  if (v1->cap.channels) {
    v1->vchannels = Mem_calloc(v1->cap.channels, sizeof(ChannelWithTuners));
    for (i=0; i < v1->cap.channels; i++) {
      v1->vchannels[i].channel.channel = i;
      rc = ioctl(v->fd, VIDIOCGCHAN, &v1->vchannels[i]);
      rc_check("VIDIOCGCHAN");

      if (0 == i) {  /* first pass only */
	v1->vchannels[0].channel.channel = 0;
	v1->vchannels[0].channel.norm = 1;
	/* Input channel: composite/svideo/tuner/etc. */
	int whichChannel = 0;	/* FIXME: allow for selecting */
	rc = ioctl(v->fd, VIDIOCSCHAN, &v1->vchannels[whichChannel]);
	rc_check("VIDIOCSCHAN");
      }

      if (0 == v1->vchannels[i].channel.tuners) {
	continue;
      }

      v1->vchannels[i].tuners = Mem_calloc(v1->vchannels[i].channel.tuners, sizeof(struct video_tuner));
      for (j=0; j < v1->vchannels[i].channel.tuners; j++) {
	v1->vchannels[i].tuners[j].tuner = j;
	rc = ioctl(v->fd, VIDIOCGTUNER, &v1->vchannels[i].tuners[j]);
	rc_check("VIDIOCGTUNER");

	/* FIXME: guess mode based on environment variables, other things. */
	v1->vchannels[i].tuners[j].mode = VIDEO_MODE_NTSC;
	rc = ioctl(v->fd, VIDIOCSTUNER, &v1->vchannels[i].tuners[j]);
	rc_check("VIDIOCSTUNER");
      }
    }
  }

  // ~/projects/bbtv/v4linput.c

  if (v1->cap.audios) {
    printf("%d audios\n", v1->cap.audios);
    v1->vaudios = Mem_calloc(v1->cap.audios, sizeof(struct video_audio));
    for (i=0; i < v1->cap.audios; i++) {
      v1->vaudios[i].audio = i;
      rc = ioctl(v->fd, VIDIOCGAUDIO, &v1->vaudios[i]);
      rc_check("VIDIOCGAUDIO");
    }
  }


  // v4l_unmute_all(v1);

  /* Get and adjust the "picture" parameters. */
  rc = ioctl(v->fd, VIDIOCGPICT, &v1->picture);
  rc_check("VIDIOCGPICT");

  printf("  brightness: %d\n"
	 "         hue: %d\n"
	 "      colour: %d\n"
	 "    contrast: %d\n"
	 "   whiteness: %d\n"
	 "       depth: %d\n",
	 v1->picture.brightness,
	 v1->picture.hue,
	 v1->picture.colour,
	 v1->picture.contrast,
	 v1->picture.whiteness,
	 v1->picture.depth);

  v->brightness = v1->picture.brightness;

#if 0
  for (i=0; i < table_size(v4l1_formats); i++) {
    if (v4l1_formats[i].enumString && i == v1->picture.palette) {
      printf("%s (%s)\n", v4l1_formats[i].enumString, v4l1_formats[i].textString);
      break;
    }
  }

  
  v1->picture.brightness = 18000;
  v1->picture.hue = 32000;
  v1->picture.colour = 15000;
  v1->picture.contrast = 4000;
  v1->picture.whiteness = 25000;
  // v1->picture.depth = 16;	/* note - can't set this arbitrarily! */
  // v1->picture.palette = VIDEO_PALETTE_RGB565;

  rc = ioctl(v->fd, VIDIOCSPICT, &v1->picture);
  rc_check("VIDIOCSPICT");

  printf("  brightness: %d\n"
	 "         hue: %d\n"
	 "      colour: %d\n"
	 "    contrast: %d\n"
	 "   whiteness: %d\n"
	 "       depth: %d\n",
	 v1->picture.brightness,
	 v1->picture.hue,
	 v1->picture.colour,
	 v1->picture.contrast,
	 v1->picture.whiteness,
	 v1->picture.depth);

#endif
  rc = ioctl(v->fd, VIDIOCGMBUF, &v1->vmbuf);
  rc_check("VIDIOCGMBUF");

  // Note, if device can capture multiple frames, they will be at v1->vmbuf.offsets[i] .
  printf("vmbuf.size=%d\n", v1->vmbuf.size);
  v1->map = mmap(0, v1->vmbuf.size, PROT_READ|PROT_WRITE,MAP_SHARED,v->fd,0);
  rc = ((long)(v1->map) == -1);
  rc_check("mmap");

  v1->next_frame = 0;
  // v1->frame.data = acm1(uint8_t, v1->cap.maxwidth * v1->cap.maxheight * 2);

  V4L1_set_w_h(v, v1->cap.maxwidth, v1->cap.maxheight);

  v1->vmmap.frame = v1->next_frame;
  /* FIXME: Make this controllable. */
  v1->vmmap.format = v1->picture.palette;

  /* Prime the first capture. */
  rc = ioctl(v->fd, VIDIOCMCAPTURE, &v1->vmmap);
  rc_check("VIDIOCMCAPTURE");

  /* No frame at start. */
  v->frame = NULL;

  v->ops = &V4L1_instance_ops;
}

typedef struct {
  void (*init)(Vid_instance *v);
} V4L1_interface;

static V4L1_interface V4L1 = {
  .init = V4L1_init,
};

/* </V4L1 code> */

/* <V4L2 code> */

/* 
 * Supported formats, in order of preference. 
 * FIXME: I initially set this up because 4:2:0 is easy to handle, and OV511 supports it,
 * but if can get better quality with 4:2:2, move that up in the priority list.
 */
VidFormat v4l2PreferredFormats[] = { 
  { .fourcc = V4L2_PIX_FMT_YUV420, .label = "YUV 4:2:0", .weight = 10},
  { .fourcc = V4L2_PIX_FMT_YUYV,   .label = "YUV 4:2:2", .weight = 5}, /* will require data shuffle
									  before YUV4MPEG! */
};

static VidFormat *v4l2_get_vidformat(uint32_t fourcc)
{
  int i;
  for (i=0; i < table_size(v4l2PreferredFormats); i++) {
    VidFormat *pf = &v4l2PreferredFormats[i];
    if (pf->fourcc == fourcc) { 
      return pf;
    }
  }
  return NULL;
}

static void v4l2_check_format(Vid_instance *v, struct v4l2_fmtdesc *fmtdesc)
{
  int i;
  for (i=0; i < table_size(v4l2PreferredFormats); i++) {
    VidFormat *pf = &v4l2PreferredFormats[i];
    if (fmtdesc->pixelformat == pf->fourcc) {
      if (!v->format) {
	/* Something is better than nothing... */
	puts("good...");
	v->format = pf;
      }
      else {
	if (pf->weight > v->format->weight) {
	  /* Better than before! */
	  puts("better...");
	  v->format = pf;
	}
      }
    }
  }
}

typedef struct {
  struct v4l2_capability cap;
  struct v4l2_pix_format pixformat;
} V4L2_instance;

static int V4L2_set_w_h(Vid_instance *v, int w, int h)
{
  puts("V4L2_set_w_h");
  V4L2_instance *v2 = v->data;

  int rc;

  struct v4l2_format format = { 
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
  };
  rc = ioctl(v->fd, VIDIOC_G_FMT, &format);
  rc_check("VIDIOC_G_FMT");
  
  //int oldWidth = format.fmt.pix.width;
  //int oldHeight = format.fmt.pix.height;
  
  //format.fmt.pix.width = w;
  //format.fmt.pix.height = h;

#if 0 
  // OV511 doesn't support this...
  rc = ioctl(v->fd, VIDIOC_TRY_FMT, &format);
  if (rc != 0) { 
    perror("VIDIOC_TRY_FMT");
    goto out;
  }
#endif

  rc = ioctl(v->fd, VIDIOC_S_FMT, &format);
  if (rc != 0) { 
    perror("VIDIOC_S_FMT");
    goto out;
  }

  memcpy(&v2->pixformat, &format.fmt.pix, sizeof(v2->pixformat));

  v->w = v2->pixformat.width;
  v->h = v2->pixformat.height;
  
 out:
  return rc;
}

static int V4L2_set_exposure(Vid_instance *v, int exposure)
{
  puts("V4L2_set_exposure");
  // V4L2_instance *v2 = v->data;
  return 0;
}

static int V4L2_get_frame(Vid_instance *v)
{
  puts("V4L2_get_frame");
  // V4L2_instance *v2 = v->data;

  ArrayU8 *img = ArrayU8_new();
  // Array.append_data(img, v1->map + v1->vmbuf.offsets[current_frame], img_size);

  v->frame = img;
  return 0;
}

_Vid_instance_ops V4L2_instance_ops = {
  .set_w_h = V4L2_set_w_h,
  .set_exposure = V4L2_set_exposure,
  .get_frame = V4L2_get_frame,
};

static void V4L2_init(Vid_instance *v, struct v4l2_capability *v4l2_caps)
{
  int rc;
  int i;

  V4L2_instance *v2 = Mem_malloc(sizeof(V4L2_instance));
  v->data = v2;

  memcpy(&v2->cap, v4l2_caps, sizeof(v2->cap));
  printf("driver=%s\n", v2->cap.driver);
  printf("card=%s\n", v2->cap.card);
  printf("bus_info=%s\n", v2->cap.bus_info);

  // FIXME: There are several format types to enumerate, see v4l2_buf_type in videodev2.h
  i = 0;
  if (v2->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
    while (1) {
      struct v4l2_fmtdesc fmtdesc = { .index = i, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
      rc = ioctl(v->fd, VIDIOC_ENUM_FMT, &fmtdesc);
      if (rc == -1) { break; }
      printf("format %d: %s (%c%c%c%c)\n"
	     , i
	     , fmtdesc.description
	     , (fmtdesc.pixelformat >> 0) & 0xff
	     , (fmtdesc.pixelformat >> 8) & 0xff
	     , (fmtdesc.pixelformat >> 16) & 0xff
	     , (fmtdesc.pixelformat >> 24) & 0xff
	     );
      v4l2_check_format(v, &fmtdesc);
      i += 1;
    }

    if (!v->format) {
      printf("no handled format!\n");
      exit(1);
    }

    struct v4l2_format format = { 
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    };
    rc = ioctl(v->fd, VIDIOC_G_FMT, &format);
    rc_check("VIDIOC_G_FMT");

    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = v->format->fourcc;

    rc = ioctl(v->fd, VIDIOC_S_FMT, &format);
    rc_check("VIDIOC_S_FMT");
    
    printf("Ok @%dx%d  fourcc=%s\n", format.fmt.pix.width, format.fmt.pix.height, 
	   v4l2_get_vidformat(format.fmt.pix.pixelformat)->label);

    memcpy(&v2->pixformat, &format.fmt.pix, sizeof(v2->pixformat));
  }

  if (v2->cap.capabilities & V4L2_CAP_STREAMING) {
    
  }
  else {
    printf("no mmap capability!\n");
    exit(1);
  }


  // if (v2->cap.capabilities & V4L2_CAP_OUTPUT) ...
  // if (v2->cap.capabilities & V4L2_CAP_OVERLAY) ...

  v->w = v2->pixformat.width;
  v->h = v2->pixformat.height;

  /* No frame at start. */
  v->frame = NULL;

  v->ops = &V4L2_instance_ops;
}

typedef struct {
  void (*init)(Vid_instance *v, struct v4l2_capability *v4l2_caps);
} V4L2_interface;

static V4L2_interface V4L2 = {
  .init = V4L2_init,
};

/* </V4L2 code> */

/* <Generic V4L code> */

static void V4L_init_vid(Vid_instance *v)
{
  int rc;
  /* Check for V4L2 compatibility. See,
     http://www.linuxtv.org/downloads/video4linux/API/V4L2_API/spec/r10850.htm */

  struct v4l2_capability v4l2_caps;
#if 0
  rc = ioctl(v->fd, VIDIOC_QUERYCAP, &v4l2_caps);
#else
  /* Hack for testing ov511. */
  rc = -1;
#endif
  if (-1 == rc) {
    /* Not V4L2 compatible, use V4L1. */
    puts("V4L1 init");
    V4L1.init(v);
  }
  else {
    puts("V4L2 init");
    V4L2.init(v, &v4l2_caps);
  }
}


void V4L1Capture_init(void)
{
  // Template_register(&Example_template);
}
