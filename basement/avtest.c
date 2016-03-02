#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* sleep */
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
//#include "CJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "V4L2Capture.h"
#include "ALSACapture.h"
//#include "TCPOutput.h"
//#include "FileOutput.h"
#include "SDLstuff.h"
#include "MjpegMux.h"
#include "DJpeg.h"
#include "Mem.h"
#include "Log.h"
#include "Cfg.h"


int main(int argc, char *argv[])
{
  int i, j;
  int T = 0;
  char *alsadev = "hw:0";
  Range *range;

  printf("This is %s\n", argv[0]);

  for (i=0; i < argc; i++) {
    if (strncmp(argv[i], "hw:", 3) == 0) {
      alsadev = argv[i];
    }
    else if (streq(argv[i], "T")) {
      T = 1;
    }
  }

  V4L2Capture_init();
  DJpeg_init();
  //ALSACapture_init();
  //MjpegMux_init();
  SDLstuff_init();

  Template_list(1);

  Instance *vc = Instantiate("V4L2Capture");
  printf("instance: %s\n", vc->label);

  int bt = 0;
  int topro = 1;
  int ov511 = 0;

  GetConfigRange(vc, "device", &range);
  for (j=0; j < range->x.strings.descriptions.things_count; j++) {
    String *s = range->x.strings.descriptions.things[j];
    printf("device %s\n", s->bytes);
  }

  int n = -1;
#if 1
  n = Range_match_substring(range, "BT878");
#endif
  if (n >= 0) {
    String *s = range->x.strings.values.things[n];
    // String *s = List_get(range->string_values, n);
    // String *sd = List_get(range->string_descriptions, n);
    printf("search for BT878 returned %d, should use device %s\n", n, s->bytes);
    bt = 1;
    SetConfig(vc, "device", s->bytes);
    //Connect(vc, "YUV422P_buffer", cj);
  }
  else if ((n = Range_match_substring(range, "gspca")) >= 0) {
    String *s = range->x.strings.values.things[n];
    topro = 1;
    /* Hm, all gspca cameras show up with the same name... */
    // ov511 = 1;
    SetConfig(vc, "device", s->bytes);
    /* After the SetConfig above, can use VIDIOC_QUERYCAP .driver result. */
  }
  else {
    //Connect(vc, "BGR3_buffer", cj);
    puts("not using BT878?");
    exit(1);
  }

  Range_clear(&range);
  GetConfigRange(vc, "format", &range);
  for (j=0; j < range->x.strings.values.things_count; j++) {
    String *s;
    s = range->x.strings.values.things[j];
    printf("format %s", s->bytes);
    if (range->x.strings.descriptions.things_count == range->x.strings.values.things_count) {
      s = range->x.strings.descriptions.things[j];
      printf(" (%s)", s->bytes);
    }
    printf("\n");
  }

  Range_clear(&range);
  GetConfigRange(vc, "input", &range);
  for (j=0; j < range->x.strings.values.things_count; j++) {
    String *s;
    s = range->x.strings.values.things[j];
    printf("input %s", s->bytes);
    if (range->x.strings.descriptions.things_count == range->x.strings.values.things_count) {
      s = range->x.strings.descriptions.things[j];
      printf(" (%s)", s->bytes);
    }
    printf("\n");
  }


  if (bt) {
    Range_clear(&range);
    GetConfigRange(vc, "brightness", &range);

    Range_clear(&range);
    GetConfigRange(vc, "contrast", &range);

    Range_clear(&range);
    GetConfigRange(vc, "saturation", &range);
  }

  if (bt) {
    SetConfig(vc, "format", "YUV422P");
    SetConfig(vc, "std", "NTSC");
    SetConfig(vc, "size", "704x480");
    if (T) {
      SetConfig(vc, "input", "Television");

      // This works, but can PCM audio be captured without unmuting?  As it turns out,
      // yes!  So mute on bt878 only affects line-out, not PCM record.
      // SetConfig(vc, "mute", "0");
    }
    else {
      SetConfig(vc, "input", "Composite1");
      SetConfig(vc, "mute", "0");

      /* I came up with these default values by viewing Wii game 
	 "My Aquarium" back and forth between direct Composite connection
	 through the capture->SDL/OpenGL path.  The gravel on tank #2 is 
	 very good to test. */
      SetConfig(vc, "brightness", "34000");
      SetConfig(vc, "contrast", "28000");
      SetConfig(vc, "saturation", "32000");
    }
  }
  else if (topro) {
    SetConfig(vc, "format", "JPEG");
    //SetConfig(vc, "size", "320x240");
    SetConfig(vc, "size", "640x480");
  }
  else if (ov511) {
    SetConfig(vc, "format", "O511"); /* from videodev2.h */
  }
  else {
    // cx88, just for testing.   The analog capture on this has been problematic.
    SetConfig(vc, "format", "BGR3");
    SetConfig(vc, "std", "NTSC-M");
    SetConfig(vc, "input", "Television");
  }

  Instance *sdl = Instantiate("SDLstuff"); 

  //SetConfig(sdl, "video", "opengl");
  //SetConfig(sdl, "video", "basic");
  //Connect(sdl, "Key_event", mj);  /* Relay keyboard input to muxer. */

  if (topro) {
    Instance *dj = Instantiate("DJpeg");
    Connect(vc, "Jpeg_buffer", dj);
    Connect(dj, "YUV422P_buffer", sdl);
    Instance_loop_thread(dj);
  }
  else {
    Connect(vc, "YUV422P_buffer", sdl);
  }

  
  /* Let the video capture run. */
  Instance_loop_thread(vc);

  /* And the video display. */
  if (!getenv("DISPLAY")) {
    fprintf(stderr, "SDL would try to use AA output, exiting...\n");
    exit(1);
  }
  Instance_loop_thread(sdl);

  /* Wait loop.  Hm, might as well run a console of sorts... */
  sleep(1);

  if (T) {
    SetConfig(vc, "brightness", "28000");
    SetConfig(vc, "saturation", "36000");
    SetConfig(vc, "contrast", "33000");
  }


  while (1) {
    char buffer[256];
    char arg1[256];
    char arg2[256];

    printf("avtest>"); fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    if (buffer[0]) {
      buffer[strlen(buffer)-1] = 0;
    }
    if (streq(buffer, "quit")) {
      break;
    }
    else if (streq(buffer, "v")) {
      cfg.verbosity += 1;
      fgets(buffer, sizeof(buffer), stdin); /* Wait for line. */
      cfg.verbosity -= 1;
    }
    else if (streq(buffer, "a")) {
      abort();
    }
    else if (streq(buffer, "e")) {
      Mem_free((void*)12345);	/* Unlikely to be an allocated address... */
    }
    else if (streq(buffer, "l")) {
      Log_dump();
    }
    else {
      n = sscanf(buffer, "%s %s", arg1, arg2);
      if (n == 2 && streq(arg1, "b")) {
	SetConfig(vc, "brightness", arg2);
      }
      else if (n == 2 && streq(arg1, "s")) {
	SetConfig(vc, "saturation", arg2);
      }
      else if (n == 2 && streq(arg1, "c")) {
	SetConfig(vc, "contrast", arg2);
      }
      else if (n == 2 && streq(arg1, "agc")) {
	SetConfig(vc, "agc", arg2);
      }
      else if (n == 2) {
	SetConfig(vc, arg1, arg2);
      }
    }
  }

  sdl->state = INSTANCE_STATE_QUIT;
  sleep(1);

  return 0;
}
