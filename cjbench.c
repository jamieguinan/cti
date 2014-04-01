#include <stdio.h>
#include <stdlib.h>
// #include <sys/time.h>
#include <string.h>
#include <time.h>
#include "CJpeg.h"
#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "JpegFiler.h"

#error This module is obsolete now, use a .cmd script instead.

extern void CJpeg_stats(Instance *pi);


int main(int argc, char *argv[])
{
  int x, y, i, j;
  printf("This is %s\n", argv[0]);

  CJpeg_init();
  DJpeg_init();
  JpegFiler_init();

  Template_list(0);

  Instance *cj = Instantiate("CJpeg");
  printf("instance: %s\n", cj->label);

  PostMessage(Config_buffer_new("quality", "25"),  &cj->inputs[0]);

  if (getenv("JF")) {
    Instance *jf = Instantiate("JpegFiler");
    Connect(cj, "Jpeg_buffer", jf);
  }

  // Instance_loop_thread(cj);

  int width = 720;
  int height = 480;
  YUVYUV422P_buffer * y422p = YUVYUV422P_buffer_new(width, height);

  srandom(12345678);			/* So should yield same result every time. */
  
  for (i=0; i < 720*480; i++) {
    y422p->y[i] = random() % 256;
  }
  for (i=0; i < 720*480/2; i++) {
    y422p->cb[i] = random() % 256;
  }
  for (i=0; i < 720*480/2; i++) {
    y422p->cr[i] = random() % 256;
  }


  for (j=0; j < 100; j++) {

    YUVYUV422P_buffer * tmp = YUVYUV422P_buffer_new(width, height);
    memcpy(tmp->y, y422p->y, width*height);
    memcpy(tmp->cb, y422p->cb, width*height/2);
    memcpy(tmp->cr, y422p->cr, width*height/2);

    /* FIXME:  Look up instead of using [N] */

    while (0) {
      usleep(1);
    }
    PostMessage(tmp, &cj->inputs[3]);
  }

  while (1) {
    char buffer[256];
    printf("cjbench>"); fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    if (buffer[0]) {
      buffer[strlen(buffer)-1] = 0;
    }
    if (streq(buffer, "quit")) {
      break;
    }
  }

  CJpeg_stats(cj);

  if (1) {
    struct timespec t;
    double avg;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    printf("%ld.%06ld seconds elapsed\n", t.tv_sec, t.tv_nsec);
    avg = t.tv_sec + (t.tv_nsec/1000000000.0);
    avg = avg/100;
    printf("%.6f average frame time\n", avg);
  }

  if (getenv("JF")) {
      sleep(1);
  }

  return 0;
}
