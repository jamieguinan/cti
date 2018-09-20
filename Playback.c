/* Search and replace "Example" with new module name. */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */

#include "CTI.h"

//enum { /* INPUT_... */ };
static Input Example_inputs[] = {
  // [ INPUT_... ] = { .type_label = "" },
};

//enum { /* OUTPUT_... */ };
static Output Example_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  // int ...;
} Example_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Example_tick(Instance *pi)
{

}

static void Example_instance_init(Instance *pi)
{
  pi->data = 0L;
}


static Template Example_template = {
  .label = "Example",
  .inputs = Example_inputs,
  .num_inputs = table_size(Example_inputs),
  .outputs = Example_outputs,
  .num_outputs = table_size(Example_outputs),
  .tick = Example_tick,
  .instance_init = Example_instance_init,
};

void Example_init(void)
{
  Template_register(&Example_template);
}
