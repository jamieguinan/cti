/* 
 * TCP output.  Instances can do one of 2 things:
 *   (1) Establish connection configured destination.
 *   (2) Listen for connection and respond to request.
 * After that, transmission should be the same, and continue
 * until an error occurs.  Hm, should be able to support
 * both modes at the same time, up to 2 connections.
 * Keep it that simple, can use repeaters on the remote
 * side if desired.
 */

#error "Use Source/Sink instead..."

#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"

enum { INPUT_ANY };
static Input TCPOutput_inputs[] = {
  [ INPUT_ANY ] = { .type_label = "Data" },
};

static Output TCPOutput_outputs[] = {
};

typedef struct {
  int listen_port;		/* listen on this port number, if configured */
  int listen_socket;

  int pull_socket;
  char *pull_name;		/* getpeername */

  char *push_destination;
  int push_socket;
  int push_connected;
} TCPOutput_private;

static void TCPOutput_tick(Instance *pi)
{
  TCPOutput_private *priv = pi->data;

  /* This function gets called repeatedly.  Manage state, listen, send, etc. */
  if (priv->push_destination && !priv->push_connected) {
    if (priv->push_socket == -1) {
      /* Create socket, non-blocking, initiate connect(). */
    }
    /*  Check for error or EALREADY. */
  }

  if (priv->listen_port != 0) {
    
  }

  if (priv->pull_socket != -1) {
    
  }
}

static void TCPOutput_loop(Instance *pi)
{
  while(1) {
    // Instance_wait(pi);
    TCPOutput_tick(pi);
  }
}

static Instance *TCPOutput_new(Template *t)
{
  Instance *pi = Mem_calloc(1, sizeof(*pi));
  int i;

  pi->label = t->label;

  copy_list(t->inputs, t->num_inputs, &pi->inputs, &pi->num_inputs);
  copy_list(t->outputs, t->num_outputs, &pi->outputs, &pi->num_outputs);

  for (i=0; i < t->num_inputs; i++) {
    pi->inputs[i].parent = pi;
  }

  pi->data = Mem_calloc(1, sizeof(TCPOutput_private));
  pi->tick = TCPOutput_tick;
  pi->loop = TCPOutput_loop;

  return pi;
}

static Template TCPOutput_template; /* Set up in _init() function. */


void TCPOutput_init(void)
{
  int i;

  /* Since can't set up some things statically, do it here. */
  TCPOutput_template.label = "TCPOutput";

  TCPOutput_template.inputs = TCPOutput_inputs;
  TCPOutput_template.num_inputs = table_size(TCPOutput_inputs);
  for (i=0; i < TCPOutput_template.num_inputs; i++) {
  }

  TCPOutput_template.outputs = TCPOutput_outputs;
  TCPOutput_template.num_outputs = table_size(TCPOutput_outputs);

  TCPOutput_template.new = TCPOutput_new;

  Template_register(&TCPOutput_template);
}
