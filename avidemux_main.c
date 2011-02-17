#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		/* sleep */
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
#include "Template.h"
#include "AVIDemux.h"

int main(int argc, char *argv[])
{
  int i;
  int T = 0;
  char *alsadev = "hw:0";

  printf("This is %s\n", argv[0]);

  for (i=0; i < argc; i++) {
    if (strncmp(argv[i], "hw:", 3) == 0) {
      alsadev = argv[i];
    }
    else if (streq(argv[i], "T")) {
      T = 1;
    }
  }

  AVIDemux_init();

  Template_list();

  Instance *ad = Instantiate("AVIDemux");
  printf("instance: %s\n", ad->label);

  SetConfig(ad, "input", "sample.avi");

  Instance_loop_thread(ad);

  /* Wait loop.  Hm, might as well run a console of sorts... */
  while (1) {
    char buffer[256];
    printf("avidemux>"); fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    if (buffer[0]) {
      buffer[strlen(buffer)-1] = 0;
    }
    if (streq(buffer, "quit")) {
      break;
    }
  }

  return 0;
}
