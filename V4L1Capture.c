/* V4L (old API, "1") code.  This is obsolete, but works great for
   OV511 cameras attached to LPD boards, and I have no plans to update
   that platform to kernels beyond 2.6.x.

   References,
     ~/projects/bbtv/v4linput.c
     ~/projects/av/soup/avplug/V4L.c
 */

#include <sys/ioctl.h>
#include <string.h>		/* memcpy */
#include <stdio.h>
#include <stdlib.h>		/* exit */
#include <sys/mman.h>		/* mmap */
#include <sys/types.h>		/* open */
#include <sys/stat.h>		/* open */
#include <fcntl.h>		/* open */


#include "CTI.h"
#include "V4L1Capture.h"
#include "Mem.h"
#include "Array.h"

#define table_size(x) (sizeof(x)/sizeof(x[0]))

#define rc_check(x)  { if (rc == -1) { perror(x); }  }

#include <sys/time.h>           /* Needed for "struct timeval" member in videodev2.h */
#include <linux/videodev.h>     /* May includes videodev2.h */

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input V4L1Capture_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output V4L1Capture_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;

  int fd;
  int w, h;
  int brightness;
  int exposure;

  int enable;			/* Set this to start capturing. */
  int msg_handled;

  struct video_capability cap;  
  struct video_mbuf vmbuf;	/* Info about mmap-able area. */
  struct video_mmap vmmap;	/* mmap configuration via ioctl */
  uint8_t *map;			/* mapped memory */
  struct video_picture picture;	/* picture, erm, parameters */
  struct video_window window;	/* needed for setting output size correctly with ov511? */
  struct video_capture capture;	/* For changing capture size. */

  int next_frame;
  ArrayU8 *frame;		/* Most recent captured frame. */
} V4L1Capture_private;

static int set_w_h(Instance *pi, const char *value);

static int set_device(Instance *pi, const char *value)
{
  V4L1Capture_private *v = (V4L1Capture_private *)pi;
  int rc;

  v->fd = open(value, O_RDWR);
  if (v->fd == -1) {
    printf("Failed to open video device.\n");
    return -1;
  }

  rc = ioctl(v->fd, VIDIOCGCAP, &v->cap);
  printf("rc=%d\n", rc);
  rc_check("VIDIOCGCCAP");

  printf("name: %s, max size: %dx%d\n", v->cap.name, v->cap.maxwidth, v->cap.maxheight);

  /* Get and adjust the "picture" parameters. */
  rc = ioctl(v->fd, VIDIOCGPICT, &v->picture);
  rc_check("VIDIOCGPICT");

  printf("  brightness: %d\n"
	 "         hue: %d\n"
	 "      colour: %d\n"
	 "    contrast: %d\n"
	 "   whiteness: %d\n"
	 "       depth: %d\n",
	 v->picture.brightness,
	 v->picture.hue,
	 v->picture.colour,
	 v->picture.contrast,
	 v->picture.whiteness,
	 v->picture.depth);

  v->brightness = v->picture.brightness;

#if 0
  for (i=0; i < table_size(v4l1_formats); i++) {
    if (v4l1_formats[i].enumString && i == v->picture.palette) {
      printf("%s (%s)\n", v4l1_formats[i].enumString, v4l1_formats[i].textString);
      break;
    }
  }

  
  v->picture.brightness = 18000;
  v->picture.hue = 32000;
  v->picture.colour = 15000;
  v->picture.contrast = 4000;
  v->picture.whiteness = 25000;
  // v->picture.depth = 16;	/* note - can't set this arbitrarily! */
  // v->picture.palette = VIDEO_PALETTE_RGB565;

  rc = ioctl(v->fd, VIDIOCSPICT, &v->picture);
  rc_check("VIDIOCSPICT");

  printf("  brightness: %d\n"
	 "         hue: %d\n"
	 "      colour: %d\n"
	 "    contrast: %d\n"
	 "   whiteness: %d\n"
	 "       depth: %d\n",
	 v->picture.brightness,
	 v->picture.hue,
	 v->picture.colour,
	 v->picture.contrast,
	 v->picture.whiteness,
	 v->picture.depth);

#endif
  rc = ioctl(v->fd, VIDIOCGMBUF, &v->vmbuf);
  rc_check("VIDIOCGMBUF");

  // Note, if device can capture multiple frames, they will be at v->vmbuf.offsets[i] .
  printf("vmbuf.size=%d\n", v->vmbuf.size);
  v->map = mmap(0, v->vmbuf.size, PROT_READ|PROT_WRITE,MAP_SHARED,v->fd,0);
  rc = ((long)(v->map) == -1);
  rc_check("mmap");

  v->next_frame = 0;
  // v->frame.data = acm1(uint8_t, v->cap.maxwidth * v->cap.maxheight * 2);

  set_w_h(pi, s(String_sprintf("%dx%d", v->cap.maxwidth, v->cap.maxheight)));

  v->vmmap.frame = v->next_frame;
  /* FIXME: Make this controllable. */
  v->vmmap.format = v->picture.palette;

  /* Prime the first capture. */
  rc = ioctl(v->fd, VIDIOCMCAPTURE, &v->vmmap);
  rc_check("VIDIOCMCAPTURE");

  /* No frame at start. */
  v->frame = NULL;

  return 0;
}


static int set_brightness(Instance *pi, const char *value)
{
  V4L1Capture_private *v = (V4L1Capture_private *)pi;
  int brightness = atoi(value);
  int rc;
  v->brightness = v->picture.brightness = brightness;
  rc = ioctl(v->fd, VIDIOCSPICT, &v->picture);
  if (rc != 0) {
    perror("VIDIOCSPICT");
  }
  return 0;
}


static int set_exposure(Instance *pi, const char *value)
{
  V4L1Capture_private *v = (V4L1Capture_private *)pi;
  int exposure = atoi(value);
  v->exposure = v->picture.whiteness = exposure;
  int rc = ioctl(v->fd, VIDIOCSPICT, &v->picture);
  if (rc != 0) {
    perror("VIDIOCSPICT");
  }

  return 0;
}

static int set_w_h(Instance *pi, const char *value)
{
  V4L1Capture_private *v = (V4L1Capture_private *)pi;
  int rc;
  int n, w, h;
  n = sscanf(value, "%dx%d", &w, &h);
  if (n != 2) {
    fprintf(stderr, "invalid size string!\n");
    return -1;
  }

  puts("V4L1_set_w_h");

  v->w = w;
  v->h = h;

  /* VIDIOCGSWIN isn't needed by some drivers (ov511), but others
     require it (zoran), so I always call it. */
  rc = ioctl(v->fd, VIDIOCGWIN, &v->window);
  rc_check("VIDIOCGWIN");
  v->window.width = v->w;
  v->window.height = v->h;
  v->window.clips = NULL;
  v->window.clipcount = 0;
  while (1) {
    rc = ioctl(v->fd, VIDIOCSWIN, &v->window);
    // rc_check("VIDIOCSWIN");
    if (rc == 0) { 
      break;
    }
    perror("VIDIOCSWIN");
  }
  printf("window: %dx%d @(%d,%d)\n", v->window.width, v->window.height,
	 v->window.x, v->window.y);

  /* ... */
  rc = ioctl(v->fd, VIDIOCGPICT, &v->picture);
  rc_check("VIDIOCGPICT");

  /* For CMCAPTURE */
  v->vmmap.width = v->w;
  v->vmmap.height = v->h;

  return 0;
}

static Config config_table[] = {
  { "device",     set_device, 0L, 0L},
  { "brightness", set_brightness, 0L, 0L},
  { "exposure",   set_exposure, 0L, 0L},
  { "size",       set_w_h,   0L, 0L},  
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void V4L1Capture_tick(Instance *pi)
{
  V4L1Capture_private *priv = (V4L1Capture_private *)pi;
  int rc;
  int wait_flag = (priv->enable ? 0 : 1);
  Handler_message *hm;
  int current_frame;

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

  V4L1Capture_private *v = priv; /* alias for code compatibility */

  current_frame = v->next_frame;

  /* If more than 1 frame, can queue up next capture before waiting for last one to complete. */
  printf("%d frames\n", v->vmbuf.frames);
  if (v->vmbuf.frames > 1) {
    v->next_frame = (v->next_frame + 1) % v->vmbuf.frames;
    v->vmmap.frame = v->next_frame;
    /* printf("capturing %dx%d\n",v->vmmap.width, v->vmmap.height); */
    rc = ioctl(v->fd, VIDIOCMCAPTURE, &v->vmmap /* .frame */);
    rc_check("VIDIOCMCAPTURE");
    
  }

  rc = ioctl(v->fd, VIDIOCSYNC, &current_frame);
  rc_check("VIDIOCSYNC");
  
  /* If only a single frame, queue the next capture now. */
  if (v->vmbuf.frames == 1) {
    rc = ioctl(v->fd, VIDIOCMCAPTURE, &v->vmmap /* .frame */);  
  }

  /* Use depth to calculate captured frame size. */
  int img_size = (v->w * v->h * v->picture.depth)/8;

  ArrayU8 *img = ArrayU8_new();
  ArrayU8_append(img, 
		 ArrayU8_temp_const(v->map + v->vmbuf.offsets[current_frame], img_size));

  if (v->frame) {
    // FIXME:  If doing mark/sweep, can leave this float.  Otherwise
    // should unref previous frame.
  }
  v->frame = img;

}

static void V4L1Capture_instance_init(Instance *pi)
{
  // V4L1Capture_private *priv = (V4L1Capture_private *)pi;
}


static Template V4L1Capture_template = {
  .label = "V4L1Capture",
  .priv_size = sizeof(V4L1Capture_private),  
  .inputs = V4L1Capture_inputs,
  .num_inputs = table_size(V4L1Capture_inputs),
  .outputs = V4L1Capture_outputs,
  .num_outputs = table_size(V4L1Capture_outputs),
  .tick = V4L1Capture_tick,
  .instance_init = V4L1Capture_instance_init,
};



void V4L1Capture_init(void)
{
  Template_register(&V4L1Capture_template);
}
