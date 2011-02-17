/* 
 * File output.
 */

#error "Use Source/Sink instead..."

#include <stdio.h>		/* fopen */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "XMixedReplace.h"
#include "Mem.h"

enum { INPUT_ANY, INPUT_XMIXEDREPLACE };
static Input FileOutput_inputs[] = {
  [ INPUT_ANY ] = { .type_label = "Data" },
  [ INPUT_XMIXEDREPLACE ] = { .type_label = "XMixedReplace_buffer", },
};

static Output FileOutput_outputs[] = {
};

typedef struct {
  char *name;
  FILE *f;
} FileOutput_private;

static int set_name(Instance *pi, const char *name)
{
  FileOutput_private *priv = pi->data;

  /* This sets the name and opens the file. */
  if (priv->name) {
    free(priv->name);
  }
  priv->name = strdup(name);

  if (priv->f) {
    fclose(priv->f);
  }
  priv->f = fopen(priv->name, "wb");
  if (!priv->f) {
    perror(priv->name);
  }

  return 0;
}

static Config config_table[] = {
  { "name",     set_name, 0L, 0L},
};


static void FileOutput_tick(Instance *pi)
{
  FileOutput_private *priv = pi->data;

  if (!priv->f) {
    /* File output not set up. */
    return;
  }

  if (pi->inputs[INPUT_ANY].msg_count) {
  }
  
  if (pi->inputs[INPUT_XMIXEDREPLACE].msg_count) {
    XMixedReplace_buffer *xmr = PopMessage(&pi->inputs[INPUT_XMIXEDREPLACE]);
    int n = fwrite(xmr->data, xmr->data_length, 1, priv->f);
    if (n != 1) {
      perror(priv->name);
    }
    XMixedReplace_buffer_discard(xmr);
  }
}

static void FileOutput_loop(Instance *pi)
{
  while(1) {
    Instance_wait(pi);
    FileOutput_tick(pi);
  }
}

static Instance *FileOutput_new(Template *t)
{
  Instance *pi = Mem_calloc(1, sizeof(*pi));
  int i;

  pi->label = t->label;

  pi->config_table = config_table;
  pi->num_config_table_entries = table_size(config_table);

  copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);
  copy_list(t->outputs, t->num_outputs, &pi->outputs, &pi->num_outputs);

  for (i=0; i < t->num_inputs; i++) {
    pi->inputs[i].parent = pi;
  }

  pi->data = Mem_calloc(1, sizeof(FileOutput_private));
  pi->tick = FileOutput_tick;
  pi->loop = FileOutput_loop;

  return pi;
}

static Template FileOutput_template; /* Set up in _init() function. */


void FileOutput_init(void)
{
  int i;

  /* Since can't set up some things statically, do it here. */
  FileOutput_template.label = "FileOutput";

  FileOutput_template.inputs = FileOutput_inputs;
  FileOutput_template.num_inputs = table_size(FileOutput_inputs);
  for (i=0; i < FileOutput_template.num_inputs; i++) {
  }

  FileOutput_template.outputs = FileOutput_outputs;
  FileOutput_template.num_outputs = table_size(FileOutput_outputs);

  FileOutput_template.new = FileOutput_new;

  Template_register(&FileOutput_template);
}
