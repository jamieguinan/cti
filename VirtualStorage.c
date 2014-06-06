/* 
 * 2014-Jun-03: This is the first CTI module does NOT have a tick
 * function, and thus no associated thread.  My initial use for it is
 * to have various instances push objects into a VirtualStorage
 * instance, with "paths" associated to the objects, then HTTPServer
 * can pull/copy objects out of it.  This will require a
 * module-specific API for pulling, which is also a new thing in CTI.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "VirtualStorage.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Jpeg_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input VirtualStorage_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

//enum { /* OUTPUT_... */ };
static Output VirtualStorage_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  Index * idx;
  Lock idx_lock;
} VirtualStorage_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void cleanup(void *old_data)
{
  /* Clean up any Resource that was allocated elsewhere in this module. 
     A Resource should have its mime type set, which allows to figure out
     the cleanup function. */
  Resource * r = old_data;
  if (streq(r->mime_type, "image/jpeg")) {
    Jpeg_buffer * old_jpeg = r->container;
    Jpeg_buffer_discard(old_jpeg);
  }
  else {
    fprintf(stderr, "cleanup: can't handle type %s\n", r->mime_type);
  }
  Mem_free(r);
}


static void Jpeg_handler(Instance *pi, void *data)
{
  VirtualStorage_private *priv = (VirtualStorage_private *)pi;
  Jpeg_buffer *jpeg = data;

  if (!jpeg->c.label || String_is_none(jpeg->c.label)) {
    fprintf(stderr, "VirtualStorage requires c.label to be set for Jpegs\n");
    Jpeg_buffer_discard(jpeg);
    return;
  }

  /* Prepare */
  Resource * r = Mem_malloc(sizeof(*r));
  Jpeg_fix(jpeg);
  r->container = jpeg;
  r->data = jpeg->data;
  r->size = jpeg->data_length;
  r->mime_type = "image/jpeg";

  /* Lock */
  Lock_acquire(&priv->idx_lock);

  /* Operate */
  int err;
  void * old_data = NULL;
  Index_replace_string(priv->idx, jpeg->c.label, r, &old_data, &err);
  if (old_data) {
    cleanup(old_data);
  }

  /* Unlock */
  Lock_release(&priv->idx_lock);
}


static void VirtualStorage_instance_init(Instance *pi)
{
  VirtualStorage_private *priv = (VirtualStorage_private *)pi;
  priv->idx = Index_new();
  Lock_init(&priv->idx_lock);
}


static Template VirtualStorage_template = {
  .label = "VirtualStorage",
  .priv_size = sizeof(VirtualStorage_private),  
  .inputs = VirtualStorage_inputs,
  .num_inputs = table_size(VirtualStorage_inputs),
  .outputs = VirtualStorage_outputs,
  .num_outputs = table_size(VirtualStorage_outputs),
  .tick = NULL,
  .instance_init = VirtualStorage_instance_init,
};

void VirtualStorage_init(void)
{
  Template_register(&VirtualStorage_template);
}


/* Public API: */
VirtualStorage * VirtualStorage_from_instance(Instance *pi)
{
  if (!streq(pi->label, VirtualStorage_template.label)) {
    fprintf(stderr, "%s: wrong type instance: %s\n", __func__, pi->label);
    return NULL;
  }

  VirtualStorage_private *priv = (VirtualStorage_private *)pi;
  return (VirtualStorage *)priv;
}

void * VirtualStorage_get(VirtualStorage * vs, String * path)
{
  VirtualStorage_private * priv = (VirtualStorage_private *)vs;
  void * result = NULL;

  /* Lock */
  Lock_acquire(&priv->idx_lock);

  /* Operate */
  int err;
  result = Index_find_string(priv->idx, path, &err);
  if (result) {
    
  }

  /* Unlock */
  Lock_release(&priv->idx_lock);

  return result;
}
