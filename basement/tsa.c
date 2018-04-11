/*
 * Read 188 byte TS packets from stdin, seach for target strings,
 * dump entire TS packet if match.
 * Example:
 *   nc -l -u 192.168.1.14 -p 6679 | ./tsa naltype7
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>             /* memmem included via _GNU_SOURCE */
#include <stdint.h>
#include <unistd.h>             /* write */
#include "../MpegTS.h"

typedef struct {
  const char * label;
  int numBytes;
  uint8_t bytes[];
} Target;

#define b(...) .numBytes = sizeof((uint8_t[]){ __VA_ARGS__ }), .bytes = { __VA_ARGS__ }

static Target target1 = { .label = "nalprefix", b(0x00,0x00,0x00,0x01) };
static Target target2 = { .label = "naltype7", b(0x00,0x00,0x00,0x01,0x27)};
Target * targets[] = { &target1, &target2 };


void usage(char * argv0)
{
  int i;
  printf("Usage: %s target\n", argv0);
  for (i=0; i < sizeof(targets)/sizeof(targets[0]); i++) {
    printf("  %s\n", targets[i]->label);
  }
}


int main(int argc, char * argv[])
{
  int i;
  int n = 0;
  uint8_t pkt[188];
  Target * target = NULL;

  if (argc != 2) {
    usage(argv[0]);
    return 1;
  }

  for (i=0; i < sizeof(targets)/sizeof(targets[0]); i++) {
    if (strcmp(argv[1], targets[i]->label) == 0) {
      target = targets[i];
      break;
    }
  }

  if (!target) {
    usage(argv[0]);
    return 1;
  }

  while (fread(pkt, 1, sizeof(pkt), stdin) == sizeof(pkt)) {
    if (MpegTS_PID(pkt) == 258) {
      uint8_t * p = memmem(pkt, sizeof(pkt), target->bytes, target->numBytes);
      if (p) {
        FILE * f = popen("hexdump -Cv", "w");
        fwrite(pkt, sizeof(pkt), 1, f);
        fclose(f);
      }
      //write(fileno(stdout), &(char[]){"\n"}, 1);
      n += 1;
    }
  }
  return 0;
}
