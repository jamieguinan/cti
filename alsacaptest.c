#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* sleep */
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
//#include "CJpeg.h"
#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "V4L2Capture.h"
#include "ALSACapture.h"
//#include "TCPOutput.h"
//#include "FileOutput.h"
#include "SDLstuff.h"
#include "MjpegMux.h"
#include "Mem.h"
#include "Log.h"
#include "Cfg.h"


int main(int argc, char *argv[])
{
  int i, j;
  char *alsadev = "hw:0";
  Range *range;

  printf("This is %s\n", argv[0]);

  for (i=0; i < argc; i++) {
    if (strncmp(argv[i], "hw:", 3) == 0) {
      alsadev = argv[i];
    }
  }

  ALSACapture_init();
  MjpegMux_init();

  Template_list(1);

  Instance *mj = Instantiate("MjpegMux");
  printf("instance: %s\n", mj->label);
  SetConfig(mj, "output", "out-%s.mjx");

  Instance_loop_thread(mj);

  Instance *ac = Instantiate("ALSACapture");
  printf("instance: %s\n", ac->label);
  SetConfig(ac, "device", alsadev);
  SetConfig(ac, "rate", "32000");
  SetConfig(ac, "channels", "2");
  SetConfig(ac, "format", "signed 16-bit little endian");


  Connect(ac, "Wav_buffer", mj);

  Instance_loop_thread(ac);

  while (1) {
    char buffer[256];
    char arg1[256];
    char arg2[256];
    int n;

    printf("alsacaptest>"); fflush(stdout);
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
  }

  return 0;
}
