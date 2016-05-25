/* 
 * gcc -O -Wall -Wno-unused-result UDPA_server.c  -o UDPA_server
 */

/* Client connection states. */
#define UDPA_CLIENT_INIT       0
#define UDPA_CLIENT_CONNECTED  1
#define UDPA_CLIENT_CLOSED     2

#include <stdio.h>		/* perror */
#include <stdlib.h>		/* system */
#include <unistd.h>		/* sleep */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>		/* uint16_t */

typedef struct {
  int state;
  int udp_socket;
  struct sockaddr_in remote;
  int seq;
} UDPAckServer;

void UDPA_send(UDPAckServer * uas, void * pkt, int length)
{
  while (1) {
    /* Send one datagram. */
    /* wait N ms for ack datagram. */
    
  }
}

void UDPA_accept(UDPAckServer * uas)
{
  /* wait for initial datagram. */
  uint8_t buffer[32000];
  unsigned int remote_len = sizeof(uas->remote);
  ssize_t n;
  n = recvfrom(uas->udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *) &uas->remote, &remote_len);
  printf("%s: %d n=%zu\n", __func__, uas->seq, n);
  uas->seq += 1;
}

void UDPA_init(UDPAckServer * uas, uint16_t port)
{
  struct sockaddr_in local = {};
  uas->udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (uas->udp_socket == -1) {
    perror("socket");
    return;
  }

  local.sin_family = AF_INET;
  local.sin_port = htons(port);
  local.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(uas->udp_socket, (struct sockaddr *)&local, sizeof(local)) == -1) {
    perror("bind");
    return;
  }
}


int main(int argc, char * argv[])
{
  UDPAckServer uas = { };
  UDPA_init(&uas, 6667);

  system("netstat -uan | grep 6667");

  while (1) {
    UDPA_accept(&uas);
  }


  return 0;
}
