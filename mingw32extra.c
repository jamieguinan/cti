#include "mingw32extra.h"

#include "Socket.h"

#include <poll.h>
#include <stdio.h>

#include <windows.h>            /* Sleep(), ... */

int readlink(const char *path, char *buf, int bufsiz)
{
  printf("%s() not implemented\n", __func__); sleep(1);
  return 0;
}

int symlink(const char *oldpath, const char *newpath)
{
  printf("%s() not implemented\n", __func__); sleep(1);
  return 0;
}

int mkfifo(const char *pathname, int mode)
{
  printf("%s() not implemented\n", __func__); sleep(1);
  return 0;
}

int sleep(unsigned int seconds)
{
  Sleep(seconds * 1000);
  return 0;
}

int chown(const char *path, int owner, int group)
{
  printf("%s() not implemented\n", __func__); sleep(1);
  return 0;
}

int poll(struct pollfd *fds, int nfds, int timeout)
{
  int i;
  int rc;

  /* Translate pollfds args to fd_set... */
  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  struct timeval tv = { .tv_sec = timeout / 1000,
                        .tv_usec = (timeout % 1000) * 1000 };

  for (i=0; i < nfds; i++) {
    if (fds[i].events & POLLIN) { FD_SET(fds[i].fd, &readfds); }
    if (fds[i].events & POLLOUT) { FD_SET(fds[i].fd, &writefds); }
    if (fds[i].events & (POLLERR|POLLHUP)) { FD_SET(fds[i].fd, &exceptfds); }
  }

  /*
   * FIXME: check file descriptors, however that is done.
   * See _kbhit docs for stdin.
   *    http://msdn2.microsoft.com/en-us/library/58w7c94c(VS.80).aspx
   * Assume stdout and files are always writable.
   */

  /* Do select call... */
  rc = select(nfds+1, &readfds, &writefds, &exceptfds, &tv);

  /* Translate fd_set back to pollfd args... */
  for (i=0; i < nfds; i++) {
    fds[i].revents = 0;
    if (FD_ISSET(fds[i].fd, &readfds)) {  fds[i].revents |= POLLIN; }
    if (FD_ISSET(fds[i].fd, &writefds)) { fds[i].revents |= POLLOUT; }
    if (FD_ISSET(fds[i].fd, &exceptfds)) {  fds[i].revents |= (POLLERR|POLLHUP); }
  }

  return rc;
}


void Socket_init()
{
  int rc;
  WSADATA data;
  rc = WSAStartup((2<<8)|2, &data);
  printf("WSAStartup returns %d\n", rc);
}

void Socket_close(Socket_instance *s)
{
  if (s->fd != -1) {
    /* http://msdn2.microsoft.com/en-us/library/ms740126.aspx */
    s->rc = closesocket(s->fd);
    s->fd = -1;
  }
}

void Socket_log_error(Socket_instance *s, const char *msg)
{
  fprintf(stderr, "%s [WSAGetLastError() returns %d]\n", msg, WSAGetLastError());
}
