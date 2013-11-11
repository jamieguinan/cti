#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* close */

#include "CTI.h"
#include "XMLMessageServer.h"
#include "SourceSink.h"
#include "socket_common.h"
#include "String.h"
#include "XmlSubset.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input XMLMessageServer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output XMLMessageServer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  listen_common lsc;
  int enable;
} XMLMessageServer_private;


static int set_enable(Instance *pi, const char *value)
{
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;

  return listen_socket_setup(&priv->lsc);
}

static Config config_table[] = {
  { "v4port", 0L, 0L, 0L, cti_set_int, offsetof(XMLMessageServer_private, lsc.port) },
  { "enable", set_enable, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void handle_client_message(IO_common *io, String *msg)
{
  // Node * top = xml_string_to_nodetree(msg);
  // Example message,
  // 
  //  <channel>
  //  <chid>ch1</chid>
  //  </channel>
  // 
  // Or,
  //  <config>
  //  <instance>channel_control</instance>
  //  <key>chid</key>
  //  <value>ch133</value>
  // </config>
  //
  // What about return values???
  // 
  // Using node_find_subnode_by_path(), xml_simple_node_value()
  // 
  // String * instance_name = ...;
  // String * key = ...;
  // String * value = ...;
  // 
  // Instance *inst = InstanceGroup_find(gig, String_new(instance_name));
  // String * resp = String_value_none();
  // if (inst) {
  //   Config_buffer *c = Config_buffer_new(s(key), s(value));
  //   /*   */
  //   PostData(c, &inst->inputs[0]);
  // }
  // else {
  // }
  //
}


static void XMLMessageServer_tick(Instance *pi)
{
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
  Handler_message *hm;
  int wait_flag = 1;
  fd_set rfds;
  fd_set wfds;
  int maxfd = 0;
  int sn;
  struct timeval tv;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  if (priv->lsc.fd > 0) {
    /* Want to handle new and existing connections, so don't block in GetData. */
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  FD_SET(priv->lsc.fd, &rfds);
  maxfd = cti_max(priv->lsc.fd, maxfd);

  tv.tv_sec = 1; tv.tv_usec = 0;
  sn = select(maxfd+1, &rfds, &wfds, 0L, &tv);

  if (sn == 0) {
    /* Nothing to do... */
    goto out;
  }

  if (FD_ISSET(priv->lsc.fd, &rfds)) {
    /* Handle client connection, then close it. */
    Comm comm = { .io.s = -1 };
    Node * resp = node_new("response");
    comm.io.s = accept(priv->lsc.fd, (struct sockaddr *)&comm.io.addr, &comm.io.addrlen);
    if (comm.io.s == -1) {    
      /* This is unlikely but possible.  If it happens, just clean up
	 and return... */
      perror("accept");
    }
    else {
      /* Read, parse, respond, close.... */
      String *message = Comm_read_string_to_zero(&comm);
      if (!String_is_none(message)) {
	/* resp = */ handle_client_message(&comm.io, message);
      }
    }
    String *resp_str = node_to_string(resp);
    Comm_write_string_with_zero(&comm, resp_str);
    String_free(&resp_str);
    Comm_close(&comm);
  }  

 out:  
  pi->counter++;
}

static void XMLMessageServer_instance_init(Instance *pi)
{
  // XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
}


static Template XMLMessageServer_template = {
  .label = "XMLMessageServer",
  .priv_size = sizeof(XMLMessageServer_private),  
  .inputs = XMLMessageServer_inputs,
  .num_inputs = table_size(XMLMessageServer_inputs),
  .outputs = XMLMessageServer_outputs,
  .num_outputs = table_size(XMLMessageServer_outputs),
  .tick = XMLMessageServer_tick,
  .instance_init = XMLMessageServer_instance_init,
};

void XMLMessageServer_init(void)
{
  Template_register(&XMLMessageServer_template);
}
