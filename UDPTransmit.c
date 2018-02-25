/* UDP transmitter. Should be able to do unicast, broadcst, or
   multicast by setting addr. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */

#include "CTI.h"
#include "UDPTransmit.h"
#include "socket_common.h"	/* all the usual headers */

typedef struct {
  Instance i;
  int socket;
  uint16_t port;
  String *addr;
  struct sockaddr_in remote;
  int logging;
} UDPTransmit_private;


static void Config_handler(Instance *pi, void *msg);
static void RawData_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_RAWDATA };
static Input UDPTransmit_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler }
  , [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler }
};

//enum { /* OUTPUT_... */ };
static Output UDPTransmit_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


static int set_port(Instance *pi, const char *value)
{
  UDPTransmit_private *priv = (UDPTransmit_private *)pi;
  priv->port = atoi(value);
  return 0;
}

static int set_addr(Instance *pi, const char *value)
{
  UDPTransmit_private *priv = (UDPTransmit_private *)pi;

  if (priv->port == 0) {
    fprintf(stderr, "%s %s: must set port first\n", __FILE__, __func__);
    return 1;
  }
  
  String_set(&priv->addr, value);

  priv->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (priv->socket == -1) {
    perror("socket");
    return 1;
  }
    
  priv->remote.sin_family = AF_INET;
  priv->remote.sin_port = htons(priv->port);

  if (inet_aton(s(priv->addr), &priv->remote.sin_addr) == 0) {
    perror("inet_aton");
    return 1;
  }

  fprintf(stderr, "%s: socket init successful\n", pi->label);
  
  return 0;  
}

static Config config_table[] = {
  {"port", set_port, 0L, 0L}
  , {"addr", set_addr, 0L, 0L}
  , {"logging", 0L, 0L, 0L, cti_set_int, offsetof(UDPTransmit_private, logging)}
};

static void RawData_handler(Instance *pi, void *data)
{
  UDPTransmit_private *priv = (UDPTransmit_private *)pi;
  RawData_buffer *raw = data;

  int n = sendto(priv->socket, raw->data, raw->data_length, 0,
		 (struct sockaddr *) &priv->remote, sizeof(priv->remote));
  if (n == -1) {
    perror("sendto");
  }

  if (priv->logging) {
    static FILE * f;
    double t;
    if (!f) {
      f = fopen("/dev/shm/udp.csv", "w");
    }
    cti_getdoubletime(&t);
    fprintf(f, "%f\n", t);
  }

  RawData_buffer_release(raw);
}

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void UDPTransmit_tick(Instance *pi)
{
  Handler_message *hm;

  while ((hm = GetData(pi, 1))) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}

static void UDPTransmit_instance_init(Instance *pi)
{
  UDPTransmit_private *priv = (UDPTransmit_private *)pi;
  priv->addr = String_value_none();
}


static Template UDPTransmit_template = {
  .label = "UDPTransmit",
  .priv_size = sizeof(UDPTransmit_private),  
  .inputs = UDPTransmit_inputs,
  .num_inputs = table_size(UDPTransmit_inputs),
  .outputs = UDPTransmit_outputs,
  .num_outputs = table_size(UDPTransmit_outputs),
  .tick = UDPTransmit_tick,
  .instance_init = UDPTransmit_instance_init,
};

void UDPTransmit_init(void)
{
  Template_register(&UDPTransmit_template);
}
