#include "cti_utils.h"
#include <time.h>               /* nanosleep */
#include <sys/time.h>           /* gettimeofday */
#include <string.h>             /* strlen */
#include <stdio.h>              /* stderr */


void cti_msleep(unsigned long msecs)
{
  struct timespec ts;
  ts.tv_sec = msecs / 1000;
  ts.tv_nsec = (msecs % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void cti_usleep(unsigned long usecs)
{
  struct timespec ts;
  ts.tv_sec = usecs / 1000000;
  ts.tv_nsec = (usecs % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

void cti_getdoubletime(double *tdest)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *tdest = (tv.tv_sec + (tv.tv_usec/1000000.0));
}

unsigned char cti_extract_hex_uchar(const char * input, int offset, int count, int *err)
{
  int i;
  unsigned char result = 0;

  if (!input || strlen(input) < (offset+count)) {
    *err = 1; return 0;
  }

  for (i=0; i < count; i++) {
    result = result * 16;
    char c = input[offset+i];
    if ('0' <= c && c <= '9') {
      result += c - '0';
    }
    else if ('a' <= c && c <= 'f') {
      result += c - 'a' + 10;
    }
    else if ('A' <= c && c <= 'F') {
      result += c - 'A' + 10;
    }
    else {
      *err = 1; return 0;
    }
  }

  *err = 0; return result;
}

unsigned char cti_extract_decimal_uchar(const char * input, int offset, int count, int *err)
{
  int i;
  unsigned char result = 0;

  if (!input || strlen(input) < (offset+count)) {
    *err = 1; return 0;
  }

  for (i=0; i < count; i++) {
    result = result * 10;
    char c = input[offset+i];
    if ('0' <= c && c <= '9') {
      result += c - '0';
    }
    else {
      *err = 1; return 0;
    }
  }

  *err = 0; return result;
}

void cti_urandom_fill(void * ptr, int size, int * err)
{
  FILE * ur = fopen("/dev/urandom", "rb");
  if (!ur) {
    fprintf(stderr, "urandom open failed\n");
    *err = 1; return;
  }

  int n = fread(ptr, size, 1, ur);
  fclose(ur);
  if (n != 1) {
    *err = 1; return;
  }

  *err = 0;
}
