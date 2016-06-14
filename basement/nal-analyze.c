#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>		/* stat */
#include <sys/stat.h>		/* stat */
#include <unistd.h>		/* stat */
#include <sys/mman.h>		/* mmap */

#define NAL_type(p) ((p)[4] & 31)

int rpi_encode_yuv_c__analysis_enabled = 0;
static void analyze_segment(uint8_t * data, int length)
{
    /* Experimental function to study output buffers returned from encoder port.*/
    if (length < 4) {
	return;
    }
    uint8_t * p = data;
    uint8_t x0001[] = { 0, 0, 0, 1};
    if (memcmp(p, x0001, 4) != 0) {
	fprintf(stderr, "not 0 0 0 1\n");
	return;
    }
    printf("F:%d NRI:%d Type:%d  segment length:%d \n",
	   (p[4] >> 7) & 1,
	   (p[4] >> 5) & 3,
	   (p[4] >> 0) & 31,
	   length
	   );
}


int main(int argc, char * argv[])
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s file.h264\n", argv[0]);
    return 1;
  }

  struct stat ifstat;
  if (stat(argv[1], &ifstat) != 0) {
    perror(argv[1]);
    return 1;
  }

  FILE *ifp = fopen(argv[1], "rb");
  if (!ifp) {
    perror(argv[1]);
    return 1;
  }

  uint8_t * data = mmap(NULL, 
			ifstat.st_size, 
			PROT_READ|PROT_WRITE, 
			MAP_PRIVATE,
			fileno(ifp),
			0);

  if ((void*)data == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  int i = 0;
  int i_start = -1;
  while (i < (ifstat.st_size-4)) {
    if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 0 && data[i+3] == 1) {
      if (i_start != -1) {
	analyze_segment(data+i_start, i - i_start);
      }
      i_start = i;
    }
    i += 1;
  }

  return 0;
}
