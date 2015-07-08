#ifndef _CTI_HTTPCLIENT_H_
#define _CTI_HTTPCLIENT_H_

#include "ArrayU8.h"

typedef struct {
} HTTPTransaction;


extern void HTTP_clear(HTTPTransaction ** t);

extern void HTTP_init_get(HTTPTransaction ** t, String * url);

extern void HTTP_init_put(HTTPTransaction ** t, String * url, ArrayU8 * postdata);

extern void HTTP_process(HTTPTransaction * t);

typedef enum {
  HTTP_STATE_INIT,
  HTTP_STATE_ACTIVE,
  HTTP_STATE_COMPLETE
} HTTP_STATE;

extern int HTTP_state(HTTPTransaction * t);


#endif
