#ifndef _SOCKET_COMMON_H_
#define _SOCKET_COMMON_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "String.h"

typedef struct {
  char v4addr[4];
  uint16_t port;
  int fd;
} listen_common;

extern int listen_socket_setup(listen_common *lsc);
// extern String * socket_read_string(socket_common *cc, int size_limit);
// extern void socket_close(socket_common *cc);

#endif
