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

/* Search and replace "XPlaneControl" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "XPlaneControl.h"
#include "Position.h"

static void Config_handler(Instance *pi, void *msg);
static void position_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_POSITION,  };
static Input XPlaneControl_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_POSITION ] = { .type_label = "Position_msg", .handler = position_handler },
};

//enum { /* OUTPUT_... */ };
static Output XPlaneControl_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} XPlaneControl_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void position_handler(Instance *pi, void *msg)
{
  // XPlaneControl_private *priv = (XPlaneControl_private *)pi;
  Position_message *pos = msg;

  Position_message_release(pos);
}


static void XPlaneControl_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void XPlaneControl_instance_init(Instance *pi)
{
  // XPlaneControl_private *priv = (XPlaneControl_private *)pi;
}


static Template XPlaneControl_template = {
  .label = "XPlaneControl",
  .priv_size = sizeof(XPlaneControl_private),
  .inputs = XPlaneControl_inputs,
  .num_inputs = table_size(XPlaneControl_inputs),
  .outputs = XPlaneControl_outputs,
  .num_outputs = table_size(XPlaneControl_outputs),
  .tick = XPlaneControl_tick,
  .instance_init = XPlaneControl_instance_init,
};

void XPlaneControl_init(void)
{
  Template_register(&XPlaneControl_template);
}

