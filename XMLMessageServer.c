/*
 * This module handles remote "config intent value" requests,
 * one message per connection.  Might add other functionality later.
 * "intent" is mapped to a instance/value pair via "intent" control
 * messages.
 */
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
  Index * intents;
} XMLMessageServer_private;


typedef struct {
  /* The intent name is stored in the containing index. */
  Instance * instance;
  String * key_str;
} Intent;


Intent * Intent_new(Instance * instance, String * key_str)
{
  Intent * i = Mem_calloc(1, sizeof(*i));
  i->instance = instance;
  i->key_str = String_dup(key_str);
  return i;
}


Intent * Intent_lookup(XMLMessageServer_private * priv, String * intent_str)
{
  int err;
  Intent * it = Index_find_string(priv->intents, intent_str, &err);
  return it;
}



static int set_v4port(Instance *pi, const char *value)
{
  /* NOTE: cannot cast int addresses to shorts in armeb, so don't
     use cti_set_*() for v4port */
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
  priv->lsc.port = atoi(value);
  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
  return listen_socket_setup(&priv->lsc);
}


static int set_intent(Instance *pi, const char *value)
{
  /* value is colon-separated, intentstring:instancename:instancekeyword */
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
  String_list * ink = String_split_s(value, ":");
  String * intent_str = String_list_get(ink, 0);
  String * instance_str = String_list_get(ink, 1);
  String * key_str = String_list_get(ink, 2);
  Instance * instance = InstanceGroup_find(gig, instance_str);

  if (instance && !String_is_none(intent_str) && !String_is_none(key_str)) {
    Intent * intent = Intent_new(instance, key_str);
    int err;
    Index_add_string(priv->intents, intent_str, intent, &err);
  }
  String_list_free(&ink);
  return 0;
}



static Config config_table[] = {
  { "v4port", set_v4port, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "intent", set_intent, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void handle_client_message(XMLMessageServer_private *priv, IO_common *io, String *msg, Node * response)
{
  /* Right now this only handles config messages. */
  Node * top = xml_string_to_nodetree(msg);

  node_fwrite(top, stderr);
  // node_add_node_and_text(response, "bonus", "1");

  /*
   * FIXME: Combine 2 calls into a single function,
   * String * instance_str = xml_find_simple_node_value_by_path(top, "config/instance") 
   */

  Node * instance_node = node_find_subnode_by_path(top, "config/instance"); /* TO BE PHASED OUT */
  Node * key_node = node_find_subnode_by_path(top, "config/key"); /* TO BE PHASED OUT */
  Node * intent_node = node_find_subnode_by_path(top, "config/intent"); /* NEW */
  Node * value_node = node_find_subnode_by_path(top, "config/value");

  if (instance_node && key_node && value_node) {
    /* TO BE PHASED OUT */
    String *instance_str = xml_simple_node_value(instance_node);
    String *key_str = xml_simple_node_value(key_node);
    String *value_str = xml_simple_node_value(value_node);

    if (!String_is_none(instance_str) &&
	!String_is_none(key_str) &&
	!String_is_none(value_str)) {
      // What about return values?  Return on "reponse" node tree.
      fprintf(stderr, "config %s %s %s\n", 
	      s(instance_str),
	      s(key_str),
	      s(value_str));
      Instance *inst = InstanceGroup_find(gig, instance_str);
      if (inst) {
	Config_buffer *c = Config_buffer_new(s(key_str), s(value_str));
	PostData(c, &inst->inputs[0]);
      }
      else {
	/* note instance not found */
      }
    }
  }
  
  else if (intent_node && value_node) {
    /* NEW */
    String *intent_str = xml_simple_node_value(intent_node);
    String *value_str = xml_simple_node_value(value_node);
    
    if (!String_is_none(intent_str) &&
	!String_is_none(value_str)) {
      // What about return values?  Return on "reponse" node tree.
      Intent *i = Intent_lookup(priv, intent_str);
      if (i) {
	Config_buffer *c = Config_buffer_new(s(i->key_str), s(value_str));
	PostData(c, &i->instance->inputs[0]);
      }
      else {
	/* note instance not found */
      }
    }
  }
  
  /* FIXME: Clean up the "top" Node and the String instances above. */
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

  if (priv->lsc.fd > 0) {
    /* Want to handle new and existing connections, so don't block in GetData. */
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (priv->lsc.fd <= 0) {
    return;
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
    // fprintf(stderr, "%s: new connection!\n", __func__);
    if (comm.io.s == -1) {    
      /* This is unlikely but possible.  If it happens, just clean up
	 and return... */
      perror("accept");
    }
    else {
      /* Read, parse, respond, close.... */
      String *message = Comm_read_string_to_byte(&comm, '$');
      if (!String_is_none(message)) {
	handle_client_message(priv, &comm.io, message, resp);
      }
    }
    String *resp_str = node_to_string(resp);
    Comm_write_string_with_byte(&comm, resp_str, '$');
    String_free(&resp_str);
    Comm_close(&comm);
  }  

 out:  
  pi->counter++;
}

static void XMLMessageServer_instance_init(Instance *pi)
{
  XMLMessageServer_private *priv = (XMLMessageServer_private *)pi;
  priv->intents = Index_new();
  
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
