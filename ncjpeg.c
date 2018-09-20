#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#define streq(a, b) (strcmp(a, b) == 0 ? 1 : 0)
#include "CJpeg.h"
#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "V4L2Capture.h"
#include "ALSACapture.h"
#include "TCPOutput.h"
#include "FileOutput.h"
#include "MjpegMux.h"


void dump_templates(void)
{
  int i, j;
  printf("Templates:\n");
  for (i=0; i < num_templates; i++) {
    printf("[%d] %s\n", i, all_templates[i]->label);
    if (1) {  // i == 0
      printf("  Inputs:\n");
      for (j=0; j < all_templates[i]->num_inputs; j++) {
	printf("  [%d] %s\n", j, all_templates[i]->inputs[j].type_label);
      }

      printf("  Outputs:\n");
      for (j=0; j < all_templates[i]->num_outputs; j++) {
	printf("  [%d] %s\n", j, all_templates[i]->outputs[j].type_label);
      }
    }
    else {
      printf("...\n");
    }
    printf("\n");
  }
}


int main(int argc, char *argv[])
{
  int i, j;
  int tv = 0;
  int T = 0;
  char *alsadev = "hw:0";

  printf("This is %s\n", argv[0]);

  for (i=0; i < argc; i++) {
    if (streq(argv[i], "tv")) {
      tv = 1;
    }
    else if (strncmp(argv[i], "hw:", 3) == 0) {
      alsadev = argv[i];
    }
    else if (streq(argv[i], "T")) {
      T = 1;
    }
  }

  CJpeg_init();
  DJpeg_init();
  V4L2Capture_init();
  ALSACapture_init();
  MjpegMux_init();
  TCPOutput_init();
  FileOutput_init();

  dump_templates();

  Instance *cj = Instantiate("CJpeg");
  printf("instance: %s\n", cj->label);

  if (0) {
    /* Test code. */
    int len;
    FILE *f = fopen("20100108-120713.jpg", "rb");
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    Jpeg_buffer * jpeg = Jpeg_buffer_new(len);
    fread(jpeg->data, len, 1, f);
    fclose(f);

    Instance *dj = Instantiate("DJpeg");
    printf("instance: %s\n", dj->label);

    Connect(dj, "RGB3_buffer", cj);

    PostMessage(jpeg, &dj->inputs[0]);
    // Or: PostMessage(jpeg, FindInput(dj, "Jpeg_buffer"));

    dj->tick(dj);
  }

  Instance_loop_thread(cj);

  if (tv) {
    Range *range;

    Instance *mj = Instantiate("MjpegMux");
    printf("instance: %s\n", mj->label);

    Instance_loop_thread(mj);

    Connect(cj, "Jpeg_buffer", mj);

    Instance *fo = Instantiate("FileOutput");
    Instance_loop_thread(fo);

    Connect(mj, "XMixedReplace_buffer", fo);
    SetConfig(fo, "name", "/tmp/out.mjpeg");
    // SetConfig(fo, "name", "/dev/null");

    Instance *vc = Instantiate("V4L2Capture");
    printf("instance: %s\n", vc->label);

    int bt = 0;

    GetConfigRange(vc, "device", &range);
    for (j=0; j < range->x.strings.descriptions.things_count; j++) {
      String *s = range->x.strings.descriptions.things[j];
      printf("device %s\n", s->bytes);
    }

    int n = Range_match_substring(range, "BT878");
    if (n >= 0) {
      String *s = range->x.strings.values.things[n];
      printf("search for BT878 returned %d, should use device %s\n", n, s->bytes);
      bt = 1;
      SetConfig(vc, "device", s->bytes);
      Connect(vc, "YUV422P_buffer", cj);
    }
    else {
      Connect(vc, "BGR3_buffer", cj);
      puts("not using BT878?");
      exit(1);
    }

    Range_clear(&range);
    GetConfigRange(vc, "format", &range);

    if (bt) {
      SetConfig(vc, "format", "YUV422P");
      SetConfig(vc, "std", "NTSC");
      if (T) {
	SetConfig(vc, "input", "Television");
	// This works, but can PCM audio be captured without unmuting?  As it turns out,
	// yes!  So mute on bt878 only affects line-out, not PCM record.
	// SetConfig(vc, "mute", "0");
      }
      else {
	SetConfig(vc, "input", "Composite1");
      }
    }
    else {
      SetConfig(vc, "format", "BGR3");
      SetConfig(vc, "std", "NTSC-M");
      SetConfig(vc, "input", "Television");
    }

    // SetConfig(vc, "brightness", "-20");

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
    SetConfig(ac, "rate", "32000");
    SetConfig(ac, "channels", "2");
    SetConfig(ac, "format", "signed 16-bit little endian");

    // Logitech...
    //SetConfig(ac, "rate", "16000");
    //SetConfig(ac, "channels", "1");
    //SetConfig(ac, "format", "signed 16-bit little endian");

    // Envy24...
    //SetConfig(ac, "rate", "32000");
    //SetConfig(ac, "channels", "12");
    //SetConfig(ac, "format", "signed 32-bit little endian");

    Instance_loop_thread(ac);

    double t1, t2;
    int num_frames = 30*3;
    vc->tick(vc);		/* First tick does setup, so don't include in timing. */

    SetConfig(cj, "quality", "80");

    cti_getdoubletime(&t1);
    for (i=0; i < num_frames; i++) {
      vc->tick(vc);		/* Capture. */
    }
    cti_getdoubletime(&t2);

    float tdiff =  (t2 - t1);
    printf("%d frames in %.4f seconds\n", num_frames, tdiff);
  }

   return 0;
}
