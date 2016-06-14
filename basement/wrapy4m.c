#include <stdio.h>
#include <stdint.h>

int main(int argc, char * argv[])
{
  int width = 0;
  int height = 0;
  char ifname[256] = {};
  char ofname[256] = {};
  FILE * ifp = NULL;
  FILE * ofp = NULL;
  int i;
  
  for (i=1; i < argc; i++) {
    if (sscanf(argv[i], "width=%d", &width) == 1) {
      continue;
    }
    else if (sscanf(argv[i], "height=%d", &height) == 1) {
      continue;
    }
    else if (sscanf(argv[i], "if=%255s", ifname) == 1) {
      continue;
    }
    else if (sscanf(argv[i], "of=%255s", ofname) == 1) {
      continue;
    }
  }

  if (ifname[0] == 0 || ofname[0] == 0 || width == 0 || height == 0) {
    fprintf(stderr, "Example usage: %s width=640 height=360 if=in.raw of=out.y4m\n", argv[0]);
    return 1;
  }

  fprintf(stderr, "%dx%d\n", width, height);

  if ((ifp = fopen(ifname, "rb")) == NULL) { perror(ifname); return 1; }
  if ((ofp = fopen(ofname, "wb")) == NULL) { perror(ofname); return 1; }

  if (fprintf(ofp, "YUV4MPEG2 W%d H%d F25:1 A1:1\n", width, height) < 0) {
    perror("fprintf");
    return 1;
  }
	      
  int framectr = 0;
  int framesize=width*height*3/2;
  while (1) {
    uint8_t buffer[framesize];
    if (fread(buffer, framesize, 1, ifp) != 1) {
      fprintf(stderr, "EOF on %s\n", ifname);
      break;
    }
    fprintf(ofp, "FRAME\n");
    if (fwrite(buffer, framesize, 1, ofp) != 1) {
      perror(ofname);
      break;
    }
    fprintf(stderr, "%d frames\n", framectr++);
  }

  fclose(ofp);
  fclose(ifp);
  
  return 0;
}
