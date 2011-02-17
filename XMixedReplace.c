#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */
#include <sys/time.h>		/* gettimeofday */

#include "XMixedReplace.h"

#define BOUNDARY "--0123456789NEXT"

static const char part_format[] = 
  "%s\r\nContent-Type: %s\r\n"
  "Timestamp:%ld.%06ld\r\n"
  "Content-Length: %d\r\n\r\n";

XMixedReplace_buffer *XMixedReplace_buffer_new(void *data, int data_length, const char *mime_type)
{
  char header[256];
  int n;

  XMixedReplace_buffer *buffer = Mem_malloc(sizeof(*buffer));

  gettimeofday(&buffer->tv, NULL);

  /* Format header... */
  n = sprintf(header, 
	      part_format, 
	      BOUNDARY, 
	      mime_type,
	      buffer->tv.tv_sec, buffer->tv.tv_usec,
	      data_length);

  if (n >= sizeof(header)) {
    fprintf(stderr, "Oops! header buffer not big enough (%d >= %d)\n", n, (int) sizeof(header));
    exit(1);
  }

  buffer->data_length = strlen(header) + data_length;
  buffer->data = Mem_malloc(buffer->data_length);
  memcpy(buffer->data, header, strlen(header));
  memcpy(buffer->data+strlen(header), data, data_length);
  
  return buffer;
}

void XMixedReplace_buffer_discard(XMixedReplace_buffer *buffer)
{
  if (buffer->data) {
    Mem_free(buffer->data);
  }
  memset(buffer, 0, sizeof(*buffer));
  Mem_free(buffer);
}
