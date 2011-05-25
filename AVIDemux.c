/* Maybe rename this "RIFFDemux"? */
#include <stdio.h>
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Mem.h"

//enum { /* INPUT_... */ };
static Input AVIDemux_inputs[] = {
  // [ INPUT_... ] = { .type_label = "" },
};

//enum { /* OUTPUT_... */ };
static Output AVIDemux_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  FILE *f;
  /* May want a data structure to encapsulate the entire RIFF contents? */
  int done;
} AVIDemux_private;

static int set_input(Instance *pi, const char *value)
{
  AVIDemux_private *priv = pi->data;
  
  if (priv->f) {
    fclose(priv->f);
  }

  priv->f = fopen(value, "rb");
  printf("file: %p\n", priv->f);
  
  return 0;
}

static Config config_table[] = {
  { "input",    set_input, 0L, 0L },
};


static void AVIDemux_tick(Instance *pi)
{
  AVIDemux_private *priv = pi->data;

  /*
   * "RIFF", LE32 length (remainder of file)
   *   "AVI " chunk ID
   *   "LIST", LE32 length
   *     "hdrl" subchunk
   *     "avih", LE32 length
   *   "LIST", LE32 length
   *     "strl" subchunk
   *     "strh", LE32 length
   *     "vids"
   *     "mjpg"
   *     "strf"
   *     "MJPG"
   *     "LIST"
   *     "strl"
   *     "strh"
   *     "auds"
   *     "strf"
   *     "IDIT"
   *     "LIST"
   *     "INFO"
   *     "ISFT"
   *     "LIST"
   *     "movi"
   *     "01wb"
   *     [audio data]
   *     "00dc"
   *     [ffd8 Jpeg marker, jpeg data, including "AVI1" tag]
   *     "01wb"
   *     [audio data]
   *     "00dc"
   *     [jpeg data]
   *     "01wb"
   *     [audio data]
   *     ...
   *     ...
   *     "idx1"
   *     [ data chunk indexes ]
   */
  
  // priv->chunkA = get_bytes(f);
  priv->done = 1;
}


static void AVIDemux_loop(Instance *pi)
{
  AVIDemux_private *priv = pi->data;

  while (1) {
    AVIDemux_tick(pi);
    if (priv->done) {
      break;
    }
  }
}


static Instance *AVIDemux_new(Template *t)
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

  AVIDemux_private *priv = Mem_calloc(1, sizeof(*priv));
  // priv->... = ...;

  pi->data = priv;
  pi->tick = AVIDemux_tick;
  pi->loop = AVIDemux_loop;

  return pi;
}

static Template AVIDemux_template; /* Set up in _init() function. */

void AVIDemux_init(void)
{
  int i;

  /* Since can't set up some things statically, do it here. */
  AVIDemux_template.label = "AVIDemux";

  AVIDemux_template.inputs = AVIDemux_inputs;
  AVIDemux_template.num_inputs = table_size(AVIDemux_inputs);
  for (i=0; i < AVIDemux_template.num_inputs; i++) {
  }

  AVIDemux_template.outputs = AVIDemux_outputs;
  AVIDemux_template.num_outputs = table_size(AVIDemux_outputs);

  AVIDemux_template.new = AVIDemux_new;

  Template_register(&AVIDemux_template);
}

