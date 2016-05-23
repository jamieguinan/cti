/* 
 * This program manages a media playback program (omxplayer, mplayer),
 * and related auxilliary programs.
 */
#include <stdlib.h>		/* getenv */
#include <unistd.h>		/* chdir */
#include <stdio.h>		/* fprintf */
#include <poll.h>		/* poll */
#include <pthread.h>		/* threads */

#include "socket_common.h"

static void * handle_ipc(void * ptr)
{
  int *pfd = ptr;
  int fd = *pfd;
  unsigned char xreq;
  int rc;
  struct pollfd fds[4];
  int nfds = 0;
  
  if (read(fd, &xreq, 1) != 1) {
    perror("read");
    return NULL;
  }

  fprintf(stderr, "fd=%d xreq=%d\n", fd, (int)xreq);

  fds[0].fd = fd;
  fds[0].events = POLLIN|POLLERR;
  nfds++;

  rc = poll(fds, nfds, -1);
  fprintf(stderr, "poll %d: ", rc);

  if (rc <= 0) {
    fprintf(stderr, "poll error ");
  }
  else if (fds[0].revents & POLLIN) {
    fprintf(stderr, "input or closed ");
  }
  else if (fds[0].revents & POLLERR) {
    fprintf(stderr, "error");
  }

  fprintf(stderr, "xreq=%d\n", (int)xreq);

  close(fd);
  free(pfd);
  return NULL;
}


static void accept_connection(int listenfd)
{
  struct sockaddr_in addr = {};
  socklen_t addrlen = sizeof(addr);
  int fd = accept(listenfd, (struct sockaddr *)&addr, &addrlen);
  if (fd == -1) {
    /* This is unlikely but possible.  If it happens, just clean up                          
       and continue. */
    perror("accept");
    return;
  }

  int *pfd = malloc(sizeof(*pfd)); *pfd = fd;

  pthread_t t;
  int rc = pthread_create(&t, NULL, handle_ipc, pfd);
  if (rc == 0) {
    pthread_detach(t);
  }
  else {
    perror("accept_connection:pthread_create");
  }
}


int main(int argc, char *argv[])
{
  int rc;

  listen_common lsc = {};
  lsc.port = 5101;
  rc = listen_socket_setup(&lsc);
  if (rc != 0) {
    goto out;
  }

  int done = 0;
  while (!done) {
    struct pollfd fds[2];
    fds[0].fd = lsc.fd;
    fds[0].events = POLLIN;

    if (poll(fds, 1, 1000) > 0) {
      if (fds[0].revents & POLLIN) {
	accept_connection(lsc.fd);
      }
    }
    /* Do things in between poll timeouts. */
    
  }

 out:
  return 0;
}
