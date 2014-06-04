/* This program is for aligning a satellite dish, with audio feedback. */
#include <stdio.h>
#include <string.h>

typedef struct {
  char *flags;
  char *signal;
  char *snr;
  char *ber;
  char *lock;
} Breakdown;

static void parse(char * buffer, Breakdown * b)
{
  char * p = buffer+7;

  b->flags = p;
  p = strstr(p, "|");
  *p = 0; p += 8;

  b->signal = p;
  p = strstr(p, "|");
  *p = 0; p += 5;

  b->snr = p;
  p = strstr(p, "|");
  *p = 0; p += 5;

  b->ber = p;
  p = strstr(p, "|");
  *p = 0;  p+=1;
  p = strstr(p, "|");
  *p = 0; p += 2;
  
  b->lock = p;
}

int main()
{
  int count = 0;
  const char *cmd = "ssh sat1w femon -H";
  FILE *p = popen(cmd, "r");

  if (!p) {
    perror(cmd);
    return 1;
  }
  
  while (1) {
    char buffer[512];
    char cmd[256];
    Breakdown b = {};

    if (fgets(buffer, sizeof(buffer), p) == NULL) {
      break;
    }
    if (count++ == 0) {
      continue;
    }
    parse(buffer, &b);
    printf("flags=%s signal=%s (%d) snr=%s (%d) ber=%s (%d) lock=%s",
	   b.flags, b.signal, atoi(b.signal), b.snr, atoi(b.snr), b.ber, atoi(b.ber), b.lock);

    int signal = atoi(b.signal);
    int snr = atoi(b.snr);
    int ber = atoi(b.ber);

    int flagadd;
    if (b.lock[0] == 'F') { 
      flagadd = 100; 
    } 
    else { 
      flagadd = 0;
    }

    int beradd = 0;
    if (signal && snr && ber == 0) {
      beradd = 100;
    }

    sprintf(cmd, "%s -D plughw:4 -t sine -p 8 -b 6553 -P 3 -f %d -l 1 1 > /dev/null 2> /dev/null",
	    "/tmp/alsa-utils-1.0.26/speaker-test/speaker-test", 200+signal);
    system(cmd);

    sprintf(cmd, "%s -D plughw:4 -t sine -p 8 -b 6553 -P 3 -f %d -l 1 > /dev/null 2> /dev/null",
	    "/tmp/alsa-utils-1.0.26/speaker-test/speaker-test", 200+snr);
    system(cmd);

    sprintf(cmd, "%s -D plughw:4 -t sine -p 8 -b 6553 -P 3 -f %d -l 1 > /dev/null 2> /dev/null",
	    "/tmp/alsa-utils-1.0.26/speaker-test/speaker-test", 200+beradd);
    system(cmd);

    sprintf(cmd, "%s -D plughw:4 -t sine -p 8 -b 6553 -P 3 -f %d -l 1 1 > /dev/null 2> /dev/null",
	    "/tmp/alsa-utils-1.0.26/speaker-test/speaker-test", 200+flagadd);
    system(cmd);

  }

  return 0;
}
