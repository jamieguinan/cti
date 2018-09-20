#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>             /* sleep */
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "VFilter.h"
#include "MjpegDemux.h"
#include "Mpeg2Enc.h"
#include "Mp2Enc.h"
#include "Mem.h"
#include "Log.h"
#include "Cfg.h"
#include "String.h"
#include "AudioLimiter.h"

extern void CJpeg_stats(Instance *pi);

int main(int argc, char *argv[])
{
  char *mjxfile = 0L;
  char *limit = "0";
  int connect = 1;
  int i;
  cfg.verbosity = 0;

  for (i=1; i < argc; i++) {
    if (strstr(argv[i], "limit=") == argv[i]) {
      limit = argv[i] + strlen("limit=");
      printf("limit set to %s\n", limit);
    }
    else if (sscanf(argv[i], "connect=%d", &connect) == 1) {
      printf("connect set to %d\n", connect);
    }
    else {
      mjxfile = argv[i];
    }
  }

  if (!mjxfile) {
    fprintf(stderr, "Error: must supply .mjx file.\n");
    return 1;
  }

  char mjxbase[256] = {};
  for (i=0; mjxfile[i]; i++) {
    if (mjxfile[i] == '.') {
      break;
    }
    mjxbase[i] = mjxfile[i];
  }

  if (mjxfile[i] != '.') {
    printf("no '.' found in mjxfile!\n");
  }

  MjpegDemux_init();
  DJpeg_init();
  VFilter_init();
  Mpeg2Enc_init();
  Mp2Enc_init();
  AudioLimiter_init();

  Template_list(0);

  Instance *vf = Instantiate("VFilter");
  Instance_loop_thread(vf);

  Instance *mjd = Instantiate("MjpegDemux");
  SetConfig(mjd, "input", mjxfile);

  Instance *dj = Instantiate("DJpeg");
  Instance_loop_thread(dj);

  Instance *mve = Instantiate("Mpeg2Enc");
  SetConfig(mve, "vout", String_sprintf("%s.m2v", mjxbase)->bytes);
  //SetConfig(mve, "vout", "test.m2v");
  Instance_loop_thread(mve);

  Instance *mae = Instantiate("Mp2Enc");
  SetConfig(mae, "aout", String_sprintf("%s.mp2", mjxbase)->bytes);
  //SetConfig(mae, "aout", "test.mp2");
  Instance_loop_thread(mae);

  if (connect) {
    Connect(mjd, "Jpeg_buffer", dj);

    if (!limit) {
      Connect(mjd, "Wav_buffer", mae);
    }
    else {
      Instance *al = Instantiate("AudioLimiter");
      SetConfig(al, "limit", limit);
      Instance_loop_thread(al);
      Connect(mjd, "Wav_buffer", al);
      Connect(al, "Wav_buffer", mae);
    }
  }

  if (1) {
    Connect(dj, "YUV422P_buffer", vf);
    /* Don't actually need this if source is 352x480. */
    // SetConfig(vf, "left_right_crop", "4");
    SetConfig(vf, "linear_blend", "1");
    Connect(vf, "YUV422P_buffer", mve);
  }
  else {
    Connect(dj, "YUV422P_buffer", mve);
  }

  Instance_loop_thread(mjd);

  /* Wait loop.  Hm, might as well run a console of sorts... */
  while (1) {
    char buffer[256];
    printf("dvdgen>"); fflush(stdout);
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
  }



  return 0;
}
