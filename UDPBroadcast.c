/* UDP transmitter. Should be able to do unicast, broadcst, or
   multicast by setting addr. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "UDPBroadcast.h"
#include "socket_common.h"	/* all the usual headers */

typedef struct {
  Instance i;
  int socket;
  uint16_t port;
  String *addr;
  struct sockaddr_in remote;  
} UDPBroadcast_private;


static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input UDPBroadcast_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output UDPBroadcast_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


static int set_port(Instance *pi, const char *value)
{
  UDPBroadcast_private *priv = (UDPBroadcast_private *)pi;
  priv->port = atoi(value);
  return 0;
}

static int set_addr(Instance *pi, const char *value)
{
  UDPBroadcast_private *priv = (UDPBroadcast_private *)pi;

  if (priv->port == 0) {
    fprintf("%s %s: must set port first\n", __FILE__, __func__);
    return 1;
  }
  
  String_set(&priv->addr, value);

  priv->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (priv->socket == -1) {
    perror("socket");
    return;
  }
    
  priv->remote.sin_family = AF_INET;
  priv->remote.sin_port = htons(priv->port);

  if (inet_aton(s(priv->addr), &priv->remote.sin_addr) == 0) {
    perror("inet_aton");
    return;
  }

  fprintf(stderr, "%s: socket init successful\n", pi->label);
  
  return 0;  
}

static Config config_table[] = {
  {"port", set_port, 0L, 0L}
  , {"addr", set_addr, 0L, 0L}
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void UDPBroadcast_tick(Instance *pi)
{
  UDPBroadcast_private *priv = (UDPBroadcast_private *)pi;
  Handler_message *hm;

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  uint8_t message[1401] = { 0 };
  int n = sendto(priv->socket, message, sizeof(message), 0,
		 (struct sockaddr *) &priv->remote, sizeof(priv->remote));
  if (n == -1) {
    perror("sendto");
  }

  pi->counter++;
}

static void UDPBroadcast_instance_init(Instance *pi)
{
  UDPBroadcast_private *priv = (UDPBroadcast_private *)pi;
  priv->addr = String_value_none();
}


static Template UDPBroadcast_template = {
  .label = "UDPBroadcast",
  .priv_size = sizeof(UDPBroadcast_private),  
  .inputs = UDPBroadcast_inputs,
  .num_inputs = table_size(UDPBroadcast_inputs),
  .outputs = UDPBroadcast_outputs,
  .num_outputs = table_size(UDPBroadcast_outputs),
  .tick = UDPBroadcast_tick,
  .instance_init = UDPBroadcast_instance_init,
};

void UDPBroadcast_init(void)
{
  Template_register(&UDPBroadcast_template);
}
