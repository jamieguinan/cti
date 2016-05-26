/* 
   gcc -O -Wall -Wno-unused-result UDPA_client.c  -o UDPA_client
 */
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
} UDPAckClient;

void UDPA_client_go(UDPAckClient * uac)
{
  /* send initial datagram. */
  uint8_t message[1401] = { 0 };
  unsigned int remote_len = sizeof(uac->remote);  
  uint8_t buffer[32000];
  struct sockaddr_in xremote;
  unsigned int xremote_len = sizeof(xremote);
  ssize_t n1, n2;
  while (1) {
    n1 = sendto(uac->udp_socket, message, sizeof(message), 0, (struct sockaddr *) &uac->remote, remote_len);
    if (n1 == -1) {
      perror("sendto");
      break;
    }

    n2 = recvfrom(uac->udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *) &uac->remote, &remote_len);
    //printf("%s: %d n1=%ld\n", __func__, uac->seq, n1);
    printf("%s: %d n2=%zu\n", __func__, uac->seq, n2);
    uac->seq += 1;
  }
}


void UDPA_client_init(UDPAckClient * uac, const char * server, uint16_t port)
{
  uac->udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (uac->udp_socket == -1) {
    perror("socket");
    return;
  }

  uac->remote.sin_family = AF_INET;
  uac->remote.sin_port = htons(port);

  if (inet_aton(server, &uac->remote.sin_addr) == 0) {
    perror("inet_aton");
    return;
  }
}


int main(int argc, char * argv[])
{
  if (argc != 2) {
    printf("Usage: %s ipaddr\n", argv[0]);
    return 1;
  }
  UDPAckClient uac = { };
  UDPA_client_init(&uac, argv[1], 6667);
  UDPA_client_go(&uac);

  return 0;
}

