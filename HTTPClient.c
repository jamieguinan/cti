#include "HTTPClient.h"
#include "Mem.h"

void HTTP_clear(HTTPTransaction ** t)
{
  if ((*t)->comm) {
    Comm_free(&((*t)->comm));
  }
  Mem_free(*t);
  *t = NULL;
}


void HTTP_init_get(HTTPTransaction ** t, String * url)
{
  if (t) {
    fprintf(stderr, "%s: warning, stale transaction struct lost\n", __func__);
  }
  *t = Mem_calloc(1, sizeof(HTTPTransaction *));
  (*t)->comm = Comm_new(s(url));
  (*t)->code = 0;
  (*t)->state = HTTP_STATE_GET_XMIT;
}


void HTTP_init_put(HTTPTransaction ** t, String * url, ArrayU8 * postdata)
{
}


void HTTP_process(HTTPTransaction * t)
{
#if 0
  // In progress, commented out so I can work on other code.

  //HTTP_STATE_NONE,
  //HTTP_STATE_INIT,
  //HTTP_STATE_ACTIVE,
  //HTTP_STATE_COMPLETE,

  if (t->comm->state == IO_CLOSED) {
    t->code = 500;
    t->state = HTTP_STATE_COMPLETE;
    return;
  }

  if (t->state == HTTP_STATE_NONE) {
    /* FIXME:  Error.  Sleep and return? */
  } 
  else if (t->state == HTTP_STATE_COMPLETE) {
    /* Nothing to do. */
  }
  else if (t->state == HTTP_STATE_GET_XMIT) {
    // ...
  }
  else if (t->state == HTTP_STATE_POST_XMIT) {
    // ...
  }
#endif
}


int HTTP_state(HTTPTransaction * t)
{
  return (t->state);
}
