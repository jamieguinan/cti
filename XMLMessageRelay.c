/* Translate messages to XML and send to host:port */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "XMLMessageRelay.h"
#include "SourceSink.h"
#include "Keycodes.h"

static void Config_handler(Instance *pi, void *msg);
static void Keycode_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input XMLMessageRelay_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_message", .handler = Keycode_handler },
};

//enum { /* OUTPUT_... */ };
static Output XMLMessageRelay_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String * destination; 	/* host:port */
} XMLMessageRelay_private;


static int set_destination(Instance * pi, const char * value)
{
  XMLMessageRelay_private *priv = (XMLMessageRelay_private *)pi;
  String_set(&priv->destination, value);
  return 0;
}


static Config config_table[] = {
  { "destination",    set_destination, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Keycode_handler(Instance *pi, void *msg)
{
  XMLMessageRelay_private *priv = (XMLMessageRelay_private *)pi;
  Keycode_message * km = msg;
  Comm * c = Comm_new(s(priv->destination));
  if (c->io.state == IO_OPEN_SOCKET) {
    char keycode_str[64]; sprintf(keycode_str, "%d", km->keycode);
    String * xmlmsg = String_sprintf("<config>\n"
				  "<instance>%s</instance>\n"
				  "<key>%s</key>\n"
				  "<value>%s</value>\n"
				  "</config>",
				  "relay",
				  "token",
				  keycode_str);
    Comm_write_string_with_byte(c, xmlmsg, '$');
    String * response = Comm_read_string_to_byte(c, '$');
    String_free(&response);
    String_free(&xmlmsg);
  }
  Keycode_message_cleanup(&km);
  Comm_free(&c);
}


static void XMLMessageRelay_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}


static void XMLMessageRelay_instance_init(Instance *pi)
{
  XMLMessageRelay_private *priv = (XMLMessageRelay_private *)pi;
  priv->destination = String_value_none();
}


static Template XMLMessageRelay_template = {
  .label = "XMLMessageRelay",
  .priv_size = sizeof(XMLMessageRelay_private),
  .inputs = XMLMessageRelay_inputs,
  .num_inputs = table_size(XMLMessageRelay_inputs),
  .outputs = XMLMessageRelay_outputs,
  .num_outputs = table_size(XMLMessageRelay_outputs),
  .tick = XMLMessageRelay_tick,
  .instance_init = XMLMessageRelay_instance_init,
};

void XMLMessageRelay_init(void)
{
  Template_register(&XMLMessageRelay_template);
}
