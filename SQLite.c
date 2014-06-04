/* Search and replace "SQLite" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "SQLite.h"
#include "sqlite3.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input SQLite_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output SQLite_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  // int ...;
} SQLite_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void SQLite_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void SQLite_instance_init(Instance *pi)
{
  // SQLite_private *priv = (SQLite_private *)pi;
}


static Template SQLite_template = {
  .label = "SQLite",
  .priv_size = sizeof(SQLite_private),  
  .inputs = SQLite_inputs,
  .num_inputs = table_size(SQLite_inputs),
  .outputs = SQLite_outputs,
  .num_outputs = table_size(SQLite_outputs),
  .tick = SQLite_tick,
  .instance_init = SQLite_instance_init,
};

void SQLite_init(void)
{
  sqlite3_initialize();
  Template_register(&SQLite_template);
}
