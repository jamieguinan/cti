#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* sleep */
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
#include "CJpeg.h"
//#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "V4L2Capture.h"
#include "ALSACapture.h"
//#include "TCPOutput.h"
//#include "FileOutput.h"
#include "MjpegMux.h"
#include "HalfWidth.h"
#include "SDLstuff.h"
#include "Mem.h"
#include "Log.h"
#include "Cfg.h"

extern void CJpeg_stats(Instance *pi);

int main(int argc, char *argv[])
{
  int i, j;
  int n;
  char *alsadev = "hw:0";
  char *mode = "none";
  char *videodev = "BT8";
  char *output = "sully:4555";
  char val[256];
  Range *range;

  printf("This is %s\n", argv[0]);

  for (i=0; i < argc; i++) {
    if (strncmp(argv[i], "hw:", 3) == 0) {
      alsadev = argv[i];
    }
    else if (sscanf(argv[i], "mode=%s", val) == 1) {
      mode = strdup(val);
    }
    else if (sscanf(argv[i], "output=%s", val) == 1) {
      output = strdup(val);
    }
    else if (sscanf(argv[i], "videodev=%s", val) == 1) {
      videodev = strdup(val);
    }
  }

  CJpeg_init();
  V4L2Capture_init();
  ALSACapture_init();
  MjpegMux_init();
  HalfWidth_init();

  Template_list(1);

  Instance *cj = Instantiate("CJpeg");
  printf("instance: %s\n", cj->label);
  Instance_loop_thread(cj);

  Instance *mj = Instantiate("MjpegMux");
  printf("instance: %s\n", mj->label);

  Instance_loop_thread(mj);

  Connect(cj, "Jpeg_buffer", mj);

  SetConfig(mj, "output", output);

  Instance *vc = Instantiate("V4L2Capture");
  printf("instance: %s\n", vc->label);

  GetConfigRange(vc, "device", &range);
  for (j=0; j < range->x.strings.descriptions.things_count; j++) {
    String *s = range->x.strings.descriptions.things[j];
    printf("device %s\n", s->bytes);
  }

  n = Range_match_substring(range, videodev);
  if (n >= 0) {
    String *s = range->x.strings.values.things[n];
    // String *s = List_get(range->string_values, n);
    // String *sd = List_get(range->string_descriptions, n);
    printf("search for %s returned %d, should use device %s\n", videodev, n, s->bytes);
    SetConfig(vc, "device", s->bytes);
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


  Range_clear(&range);
  GetConfigRange(vc, "brightness", &range);

  Range_clear(&range);
  GetConfigRange(vc, "contrast", &range);

  Range_clear(&range);
  GetConfigRange(vc, "saturation", &range);

  if (streq(mode, "vcr") || streq(mode, "wii") || streq(mode, "sullytv")) {

    SetConfig(vc, "format", "YUV422P");
    SetConfig(vc, "std", "NTSC");

    if (streq(mode, "vcr") || streq(mode, "wii")) {
      /* Set mixer values.  */
      system("amixer sset Line 25%,25% cap");
      system("amixer sset Capture 25%,25% cap");

      if (streq(mode, "vcr")) {
	/* Insert a half-width scaler. */
	Instance *hw = Instantiate("HalfWidth");
	Connect(vc, "YUV422P_buffer", hw);
	Connect(hw, "YUV422P_buffer", cj);
	Instance_loop_thread(hw);
      }
      else { /* wii */
	Connect(vc, "YUV422P_buffer", cj);
      }

      SetConfig(vc, "size", "704x480");
      SetConfig(vc, "input", "Composite1");
      SetConfig(vc, "mute", "0");

      SetConfig(vc, "brightness", "30000");
      SetConfig(vc, "contrast", "32000");
      SetConfig(vc, "saturation", "32000");
    }
    else if (streq(mode, "sullytv")) {
      /* Insert a half-width scaler. */
#if 0
      Instance *hw = Instantiate("HalfWidth");
      Connect(vc, "YUV422P_buffer", hw);
      Connect(hw, "YUV422P_buffer", cj);
      Instance_loop_thread(hw);
#else
      Connect(vc, "YUV422P_buffer", cj);
#endif
      SetConfig(vc, "input", "Television");
      SetConfig(vc, "size", "704x480");
      // This works, but can PCM audio be captured without unmuting?  As it turns out,
      // yes!  So mute on bt878 only affects line-out, not PCM record.
      // SetConfig(vc, "mute", "0");
    }
  }
  else if (streq(mode, "cx88")) {
    // cx88, just for testing.   The analog capture on this has been problematic.
    SetConfig(vc, "format", "BGR3");
    SetConfig(vc, "std", "NTSC-M");
    SetConfig(vc, "input", "Television");
  }

  else if (streq(mode, "logitech")) {
    printf("logitech format...\n");
    SetConfig(vc, "format", "MJPG");
    SetConfig(vc, "size", "800x600");
    Connect(vc, "Jpeg_buffer", mj);
  }

  else {
    printf("unknown mode: %s\n", mode);
    exit(1);
  }

  Instance *ac = Instantiate("ALSACapture");
  printf("instance: %s\n", ac->label);

  SetConfig(ac, "device", alsadev);

  Range_clear(&range);
  GetConfigRange(ac, "rate", &range);

  Range_clear(&range);
  GetConfigRange(ac, "channels", &range);

  Range_clear(&range);
  GetConfigRange(ac, "format", &range);

  Connect(ac, "Wav_buffer", mj);

  // Bt878...
  //SetConfig(ac, "rate", "32000");
  //SetConfig(ac, "channels", "2");
  //SetConfig(ac, "format", "signed 16-bit little endian");

  // Logitech...
  SetConfig(ac, "rate", "16000");
  SetConfig(ac, "channels", "1");
  SetConfig(ac, "format", "signed 16-bit little endian");

  // Envy24...
  //SetConfig(ac, "rate", "32000");
  //SetConfig(ac, "channels", "12");
  //SetConfig(ac, "format", "signed 32-bit little endian");

  //SetConfig(cj, "quality", "80");
  SetConfig(cj, "quality", "75");
  //SetConfig(cj, "quality", "50");

  //sdl = SDLstuff_new();  /* This should pop up a window, which should be left in the foreground
  //                          to receive keyboard events. */
  //SetConfig(sdl, "video", "opengl");
  //SetConfig(sdl, "video", "basic");
  //Connect(sdl, "Key_event", mj);  /* Relay keyboard input to muxer. */

  //kb = XKBflasher_new();
  //Connect(mj, "Status_event", kb);
  //Instance_loop_thread(kb);

  /* Let the audio and video capture run. */
  Instance_loop_thread(ac);
  Instance_loop_thread(vc);

  /* Wait loop.  Hm, might as well run a console of sorts... */
  sleep(1);

  while (1) {
    char buffer[256];
    char arg1[256];
    char arg2[256];

    printf("avcap>"); fflush(stdout);
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
      else if (n == 2 && streq(arg1, "ae")) {
	SetConfig(vc, "autoexpose", arg2);
      }
      else if (n == 2 && streq(arg1, "e")) {
	SetConfig(vc, "exposure", arg2);
      }
      else if (n == 2) {
	/* This should look up the named parameter and try to set it. */
	SetConfig(vc, arg1, arg2);
      }
    }
  }

  CJpeg_stats(cj);

  return 0;
}
