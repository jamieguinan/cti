/*
 * Control "azap" for QAM tuning.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "TunerControl.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input TunerControl_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output TunerControl_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String * command;
  String * dbschema;
  String * dbfile;
  // SQLite * db;
} TunerControl_private;


static int set_command(Instance *pi, const char *value)
{
  TunerControl_private *priv = (TunerControl_private *)pi;
  String_set(&priv->command, value);
  return 0;
}


static int set_dbschema(Instance *pi, const char *value)
{
  TunerControl_private *priv = (TunerControl_private *)pi;
  String_set(&priv->dbschema, value);
  return 0;
}


static int set_dbfile(Instance *pi, const char *value)
{
  TunerControl_private *priv = (TunerControl_private *)pi;
  String_set(&priv->dbfile, value);

  if (priv->dbschema == String_value_none()) {
    fprintf(stderr, "schema is not set!\n");
    return 1;
  }

  // priv->db = SQLite_open(priv->dbschema, priv->dbfile);

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  //TunerControl_private *priv = (TunerControl_private *)pi;
  return 0;
}


static Config config_table[] = {
  { "command", set_command, 0L, 0L },
  { "dbfile", set_dbfile, 0L, 0L },
  { "dbschema", set_dbschema, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void TunerControl_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void TunerControl_instance_init(Instance *pi)
{
  TunerControl_private *priv = (TunerControl_private *)pi;
  priv->command = String_value_none();
  priv->dbschema = String_value_none();
  priv->dbfile = String_value_none();
}


static Template TunerControl_template = {
  .label = "TunerControl",
  .priv_size = sizeof(TunerControl_private),
  .inputs = TunerControl_inputs,
  .num_inputs = table_size(TunerControl_inputs),
  .outputs = TunerControl_outputs,
  .num_outputs = table_size(TunerControl_outputs),
  .tick = TunerControl_tick,
  .instance_init = TunerControl_instance_init,
};

void TunerControl_init(void)
{
  Template_register(&TunerControl_template);
}
