/* Common socket-related code useful to more than one module. */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>		/* close, fcntl */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>		/* fcntl */


#include "socket_common.h"

/* FIXME: See arch-specific define over in modc code... */
#ifndef set_reuseaddr
#define set_reuseaddr 1
#endif

void set_cloexec(int fd)
{
  /* Prevent listen socket from being inherited by child processes. */
  int flags;
  flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    perror("fcntl F_GETFD");
  }
  else {
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
      perror("fcntl F_SETFD FD_CLOEXEC");
    }
  }
}


int listen_socket_setup(listen_common *lsc)
{
  int rc;

  lsc->fd = socket(AF_INET, SOCK_STREAM, 0);

  if (lsc->fd == -1) {
    /* FIXME: see Socket.log_error in modc code */
    fprintf(stderr, "socket error\n");
    return 1;
  }

  struct sockaddr_in sa = { .sin_family = AF_INET,
			    .sin_port = htons(lsc->port),
			    .sin_addr = { .s_addr = htonl(INADDR_ANY)}
  };

  if (set_reuseaddr) {
    int reuse = 1;
    rc = setsockopt(lsc->fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
    if (rc == -1) { 
      // Socket.log_error(s, "SO_REUSEADDR"); 
      fprintf(stderr, "SO_REUSEADDR\n"); 
      close(lsc->fd); lsc->fd = -1;
      return 1;
    }
  }

  set_cloexec(lsc->fd);

  rc = bind(lsc->fd, (struct sockaddr *)&sa, sizeof(sa));
  if (rc == -1) { 
    /* FIXME: see Socket.log_error in modc code */
    perror("bind"); 
    close(lsc->fd); lsc->fd = -1;
    return 1;
  }

  rc = listen(lsc->fd, 5);
  if (rc == -1) { 
    /* FIXME: see Socket.log_error in modc code */
    fprintf(stderr, "listen\n"); 
    close(lsc->fd); lsc->fd = -1;
    return 1;
  }

  fprintf(stderr, "listening on port %d\n", lsc->port);
  
  return 0;
}

