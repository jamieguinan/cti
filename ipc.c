/* 
 * Socket/file-descriptor based interprocess communication.
 * This is rather low-level, uses malloc/free and bare read/write calls.
 */
#include <stdio.h>		/* fprintf */
#include <string.h>		/* memcpy xo*/
#include <stdint.h>		/* int types */
#include <stdlib.h>		/* malloc */
#include <unistd.h>		/* read, write */
#include <sys/uio.h>		/* writev */
#include <poll.h>		/* poll */

#include "ipc.h"

int ipc_debug_recv_checksum;

int writable(int fd, int timeout_ms)
{
  struct pollfd fds[1] = { { .fd = fd, .events = POLLOUT } } ;
  int rc = poll(fds, 1, timeout_ms);
  if (rc != 1) {
    return 0;
  }
  return 1;
}

int readable(int fd, int timeout_ms)
{
  struct pollfd fds[1] = { { .fd = fd, .events = POLLIN } };
  int rc = poll(fds, 1, timeout_ms);
  if (rc != 1) {
    return 0;
  }
  return 1;
}

int ipc_send(int fd, uint8_t * message, uint32_t message_length, int timeout_ms)
{
  int message_written = 0;
  int n;

  if (message_length <= 250) {
    uint8_t msglen = message_length;
    
    struct iovec iov[2] = { { .iov_base = &msglen, .iov_len = 1 },
			    { .iov_base = message, .iov_len = message_length } };
    if (!writable(fd, timeout_ms)) { fprintf(stderr, "%s: not writable\n", __func__); return 1; }
    n = writev(fd, iov, 2);
    if (n < 1) { fprintf(stderr, "%s: writev case 1 failed\n", __func__); return 1; }
    message_written += (n-1);
  }
  else {
    uint8_t code = 251;
    uint8_t msglen32[4];
    msglen32[0] = (message_length >> 0) & 0xff;
    msglen32[1] = (message_length >> 8) & 0xff;
    msglen32[2] = (message_length >> 16) & 0xff;
    msglen32[3] = (message_length >> 24) & 0xff;
    
    struct iovec iov[3] = { { .iov_base = &code, .iov_len = 1 },
			    { .iov_base = &msglen32, .iov_len = 4 },
			    { .iov_base = message, .iov_len = message_length } };
    if (!writable(fd, timeout_ms)) { fprintf(stderr, "%s: not writable\n", __func__); return 1; }
    n = writev(fd, iov, 3);
    if (n < 1) { fprintf(stderr, "%s: writev case 2 failed\n", __func__); return 1; }
    else if (n < 5) {
      /* Rare case, sent code but only sent part of length. Send remainder of length. */
      int ii;
      for (ii = (n - 1); ii<4; ii+=n) {
	n = write(fd, msglen32+ii, (4-ii));
	if (n < 1) { fprintf(stderr, "%s: write message length remainder failed\n", __func__); return 1; }
      }
    }
    else {
      message_written += (n-5);
    }
  }

  while (message_written < message_length) {
    if (!writable(fd, timeout_ms)) {  fprintf(stderr, "%s: not writable\n", __func__); return 1; }
    n = write(fd, message+message_written, message_length - message_written);
    if (n < 1) { fprintf(stderr, "%s: write message data failed\n", __func__); return 1; }
    message_written += 1;
  }

  return 0;
}


int ipc_recv(int fd, uint8_t ** message, uint32_t * message_length, int timeout_ms)
{
  uint8_t buffer[32000];
  uint8_t code;
  int n;
  uint8_t * result;
  uint32_t result_length = 0;
  int bytes_read = 0;
  int message_read = 0;

  if (*message != NULL) {
    fprintf(stderr, "%s: warning, pmessage should be initialized to NULL\n", __func__);
  }

  if (!readable(fd, timeout_ms)) { fprintf(stderr, "%s: case 1 not readable\n", __func__); return 1; }
  n = read(fd, buffer, sizeof(buffer));
  if (n < 1) { fprintf(stderr, "%s: read case 1 failed\n", __func__); return 1; } 
  bytes_read = n;
  code = buffer[0];
  if (code <= 250) {
    result_length = code;
    result = malloc(result_length);
    message_read = (n-1);
    memcpy(result, buffer+1, message_read);
  }
  else if (code == 251) {
    /* Read message length into buffer. */
    if (0) printf("%s: reading message length, bytes_read=%d\n", __func__, bytes_read);
    while (bytes_read < 5) {
      if (!readable(fd, timeout_ms)) { fprintf(stderr, "%s: case 2 not readable\n", __func__); return 1; }
      n = read(fd, buffer+bytes_read, sizeof(buffer)-bytes_read);
      if (n < 1) { fprintf(stderr, "%s: read case 2 failed\n", __func__); return 1; }
      bytes_read += n;
    }
    /* Copy remainder of buffer to allocated message. */
    result_length = buffer[1] 
      + (buffer[2] << 8)
      + (buffer[3] << 16)
      + (buffer[4] << 24);
    if (0) printf("%s: result_length=%d\n", __func__, result_length);
    result = malloc(result_length);
    message_read = (bytes_read - 5);
    if (0) printf("%s: result_length=%d message_read=%d (so far)\n", __func__, result_length, message_read);
    memcpy(result, buffer+5, message_read);
  }
  else {
    fprintf(stderr, "%s: unhandled code %d\n", __func__, code); return 1;
  }

  while (message_read < result_length) {
    if (!readable(fd, timeout_ms)) { fprintf(stderr, "%s: case 3 not readable\n", __func__); free(result); return 1; }
    n = read(fd, result+message_read, result_length-message_read);
    if (n < 1) { fprintf(stderr, "%s: read case 3 failed\n", __func__); free(result); return 1; }
    message_read += n;
  }

  if (ipc_debug_recv_checksum) { 
    int i;
    int chksum = 0;
    for (i=0; i < result_length; i++) {
      chksum = chksum + result[i];
    }
    printf("recv checksum=%d\n", chksum);
  }

  *message = result;
  *message_length = result_length;
  
  return 0;
}
