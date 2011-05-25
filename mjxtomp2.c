#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* sleep */
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

extern void CJpeg_stats(Instance *pi);

int main(int argc, char *argv[])
{
  char *mjxfile = 0L;
  int i;

  for (i=1; i < argc; i++) {
    mjxfile = argv[i];
  }

  if (!mjxfile) {
    fprintf(stderr, "Error: must supply .mjx file.\n");
    return 1;
  }

  MjpegDemux_init();
  Mp2Enc_init();

  Template_list();

  Instance *mjd = Instantiate("MjpegDemux");
  SetConfig(mjd, "input", mjxfile);

  Instance *mae = Instantiate("Mp2Enc");
  SetConfig(mae, "aout", "test.mp2");
  Instance_loop_thread(mae);

  Connect(mjd, "Wav_buffer", mae);

  Instance_loop_thread(mjd);

  /* Wait loop.  Hm, might as well run a console of sorts... */
  while (1) {
    char buffer[256];
    printf("mjxtomp2>"); fflush(stdout);
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
