#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "../String.h"
#include "../Images.h"
#include "../jpeg_misc.h"
#include "../cti_utils.h"

extern int jpeg_misc_debug_save;

int main(int argc, char *argv[])
{
  struct stat st;
  uint8_t * data;
  int i, n;
  int mode;
  char * modestr = "int";  
  char * filename;
  int convert=0;

  if (argc == 2) { filename = argv[1]; }
  else if (argc == 3) { modestr = argv[1]; filename = argv[2]; }
  else {
    printf("Usage: %s [int|fast|float] jpegfile\n", argv[0]);
    return 1;
  }

  if (getenv("CONVERT")) { convert=1; }

  if (streq(modestr, "int") || streq(modestr, "fast") || streq(modestr, "float")) {
    Jpeg_dct_method(modestr);
  }
  else {
    printf("invalid dct method string %s\b", modestr);
    return 1;
  }

  if (stat(filename, &st) != 0) { perror(filename); return 1; }
  
  FILE * f = fopen(filename, "rb");
  if (!f) { perror(filename); return 1; }

  data = malloc(st.st_size);
  n = fread(data, st.st_size, 1, f);
  fclose(f);
  if (n != 1) {
    perror("fread");
    return 1;
  }

  printf("ready 1\n");
  Jpeg_buffer * jpeg = Jpeg_buffer_from(data, st.st_size, NULL);
  if (!jpeg) { return 1; }
  printf("jpeg %p\n", jpeg);

  FormatInfo * fmt = NULL;
  double t0, t;
  cti_getdoubletime(&t0);
  debug_save = 0;  
  n = 50;
  for (i=0; i < n; i++) {
    YUV420P_buffer * y420p = NULL;
    YUV422P_buffer * y422p = NULL;
    Jpeg_decompress(jpeg, &y420p, &y422p, &fmt);
    // printf("decompress Ok\n");
    if (y420p) { YUV420P_buffer_release(y420p);  }
    // printf("release 1\n");
    if (y422p && !y420p && convert) {
      y420p = YUV422P_to_YUV420P(y422p);
    }

    if (y420p) { YUV420P_buffer_release(y420p);  }
    if (y422p) { YUV422P_buffer_release(y422p);  }    
    // printf("release 2\n");

    
    if (i == (n-2)) {
      cti_getdoubletime(&t);
      debug_save = 1;
    }
  }
  
  printf("%.03fs per frame\n", (t-t0)/(n-1));
   
  return 0;
}
