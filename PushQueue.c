#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Mem.h"
#include "PushQueue.h"
#include "localptr.h"
#include "serf_get.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);
static void Push_data_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_PUSH_DATA };
static Input PushQueue_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_PUSH_DATA ] = { .type_label = "Push_data", .handler = Push_data_handler },
};

//enum { /* OUTPUT_... */ };
static Output PushQueue_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  IndexedSet(PushQueue_message) messages;
  String * uri;
  String * service_key;
} PushQueue_private;

static Config config_table[] = {
  { "uri",  0L, 0L, 0L, cti_set_string, offsetof(PushQueue_private, uri) },
  { "service_key",  0L, 0L, 0L, cti_set_string, offsetof(PushQueue_private, service_key) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Push_data_handler(Instance *pi, void *msg)
{
  PushQueue_private *priv = (PushQueue_private *)pi;
  PushQueue_message *pqm = msg;

  IndexedSet_add(priv->messages, pqm);
}

static void run_queue(PushQueue_private *priv)
{
  if (priv->messages.count <= 0) {
    return;
  }
  localptr(PushQueue_message, pqm);
  IndexedSet_pop(priv->messages, pqm);

  localptr(String, service_key_string) = File_load_text(priv->service_key);
  if (String_is_none(service_key_string)) {
    fprintf(stderr, "%s: missing service key\n", __func__);
    return;
  }
  String_trim_right(service_key_string);

  /* The headers here correspond to code in "ws.c" */
  localptr(String, cmd) = String_sprintf("serf_get"
					 " -rX-Service-Key:%s"
					 " -rX-Service-File:%s"
					 "%s%s",
					 s(service_key_string),
					 s(pqm->file_to_send),
					 String_is_none(pqm->file_to_delete) ? "":" -rX-Service-Delete:",
					 String_is_none(pqm->file_to_delete) ? "":s(pqm->file_to_delete));

  int rc = serf_command_post_data_file(cmd, priv->uri, pqm->local_path, NULL);
  if (rc != 0) {
    fprintf(stderr, "%s: serf error %d\n", __func__, rc);
  }
}

static void PushQueue_tick(Instance *pi)
{
  Handler_message *hm;
  PushQueue_private *priv = (PushQueue_private *)pi;

  if (priv->messages.count == 0) {
    hm = GetData(pi, 1);
  }
  else {
    hm = GetData(pi, 0);
  }

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  run_queue(priv);

  pi->counter++;
}

static void PushQueue_instance_init(Instance *pi)
{
  PushQueue_private *priv = (PushQueue_private *)pi;
  priv->uri = String_value_none();
  priv->service_key = String_value_none();
}


static Template PushQueue_template = {
  .label = "PushQueue",
  .priv_size = sizeof(PushQueue_private),
  .inputs = PushQueue_inputs,
  .num_inputs = table_size(PushQueue_inputs),
  .outputs = PushQueue_outputs,
  .num_outputs = table_size(PushQueue_outputs),
  .tick = PushQueue_tick,
  .instance_init = PushQueue_instance_init,
};

void PushQueue_init(void)
{
  Template_register(&PushQueue_template);
}


/* Direct API */
PushQueue_message * PushQueue_message_new(void)
{
  PushQueue_message * msg = Mem_calloc(1, sizeof(*msg));
  msg->local_path = String_value_none();
  msg->file_to_send = String_value_none();
  msg->file_to_delete = String_value_none();
  return msg;
}

void PushQueue_message_free(PushQueue_message ** msg)
{
  if (!msg) {
    fprintf(stderr, "%s: bug, unexpected NULL\n", __func__);
    return;
  }
  if (!*msg) {
    return;
  }

  if ((*msg)->local_path) {
    String_free(&(*msg)->local_path);
  }

  if ((*msg)->file_to_delete) {
    String_free(&(*msg)->file_to_delete);
  }

  if ((*msg)->file_to_send) {
    String_free(&(*msg)->file_to_send);
  }

  Mem_free(*msg);
  *msg = NULL;
}
