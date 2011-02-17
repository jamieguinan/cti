/* 
 * File input.
 */

#error "Use Source/Sink instead..."

#include <stdio.h>		/* fopen */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "XMixedReplace.h"
#include "Mem.h"

static Input FileInput_inputs[] = {
};

static Input FileInput_outputs[] = {
};

typedef struct {
  char *name;
  FILE *f;
} FileInput_private;

static int set_name(Instance *pi, const char *name)
{
  FileInput_private *priv = pi->data;

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


static void FileInput_tick(Instance *pi)
{
  FileInput_private *priv = pi->data;

  if (!priv->f) {
    /* File input not set up. */
    return;
  }
}

static void FileInput_loop(Instance *pi)
{
  while(1) {
    Instance_wait(pi);
    FileInput_tick(pi);
  }
}

static Instance *FileInput_new(Template *t)
{
  Instance *pi = Mem_calloc(1, sizeof(*pi));
  int i;

  pi->label = t->label;

  pi->config_table = config_table;
  pi->num_config_table_entries = table_size(config_table);

  copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);
  copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);

  for (i=0; i < t->num_inputs; i++) {
    pi->inputs[i].parent = pi;
  }

  pi->data = Mem_calloc(1, sizeof(FileInput_private));
  pi->tick = FileInput_tick;
  pi->loop = FileInput_loop;

  return pi;
}

static Template FileInput_template; /* Set up in _init() function. */


void FileInput_init(void)
{
  int i;

  /* Since can't set up some things statically, do it here. */
  FileInput_template.label = "FileInput";

  FileInput_template.inputs = FileInput_inputs;
  FileInput_template.num_inputs = table_size(FileInput_inputs);
  for (i=0; i < FileInput_template.num_inputs; i++) {
  }

  FileInput_template.inputs = FileInput_inputs;
  FileInput_template.num_inputs = table_size(FileInput_inputs);

  FileInput_template.new = FileInput_new;

  Template_register(&FileInput_template);
}
