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
  int family = AF_INET6;

  lsc->fd = socket(family, SOCK_STREAM, 0);

  if (lsc->fd == -1) {
    /* Maybe IPv6 isn't supported, fall back to IPv4. */
    family = AF_INET;
    lsc->fd = socket(family, SOCK_STREAM, 0);
  }

  if (lsc->fd == -1) {
    perror("socket");
    return 1;
  }

  if (set_reuseaddr) {
    int reuse = 1;
    rc = setsockopt(lsc->fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
    if (rc == -1) { 
      perror("SO_REUSEADDR\n"); 
      close(lsc->fd); lsc->fd = -1;
      return 1;
    }
  }

  set_cloexec(lsc->fd);


  if (family == AF_INET6) {
    /* http://stackoverflow.com/questions/1618240/how-to-support-both-ipv4-and-ipv6-connections */
    int x = 0;
    rc = setsockopt(lsc->fd, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&x, sizeof(x));
    if (rc == -1) { 
      perror("IPPROTO_IPV6\n"); 
    }

    struct sockaddr_in6 sa6 = {
      .sin6_family = family,
      .sin6_port = htons(lsc->port),
      .sin6_addr = IN6ADDR_ANY_INIT
    };
    rc = bind(lsc->fd, (struct sockaddr *)&sa6, sizeof(sa6));
  }
  else {
    struct sockaddr_in sa = {
      .sin_family = family,
      .sin_port = htons(lsc->port),
      .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    rc = bind(lsc->fd, (struct sockaddr *)&sa, sizeof(sa));
  }

  if (rc == -1) { 
    perror("bind"); 
    close(lsc->fd); lsc->fd = -1;
    return 1;
  }

  rc = listen(lsc->fd, 5);
  if (rc == -1) { 
    perror("listen"); 
    close(lsc->fd); lsc->fd = -1;
    return 1;
  }

  fprintf(stderr, "listening on port %d\n", lsc->port);
  
  return 0;
}

