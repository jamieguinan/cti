#include <arpa/inet.h>		/* htons */
#include <string.h>		/* memset */

#include <sys/types.h>		/* sendto */
#include <sys/socket.h>

#if 0

struct sockaddr_in addr_otherguy;
memset(&addr_otherguy,0,sizeof(addr_otherguy));
addr_otherguy.sin_family =AF_INET;
addr_otherguy.sin_port =htons(str_to_int(net.their_port_str[ip_index_to]));
addr_otherguy.sin_addr.s_addr =their_address[ip_index_to];
xint send_failed=(sendto(my_socket,data_send,data_len,0,(const struct sockaddr*)&addr_otherguy,sizeof(struct sockaddr_in))!=data_len);

#endif
