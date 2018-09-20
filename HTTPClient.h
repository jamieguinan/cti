#ifndef _CTI_HTTPCLIENT_H_
#define _CTI_HTTPCLIENT_H_

#include "ArrayU8.h"
#include "SourceSink.h"

typedef enum {
  HTTP_STATE_NONE,
  HTTP_STATE_INIT,
  HTTP_STATE_GET_XMIT,
  HTTP_STATE_POST_XMIT,
  HTTP_STATE_REPLY,
  HTTP_STATE_COMPLETE,
} HTTP_STATE;

typedef struct {
  Comm * comm;
  int code;                     /* 200, 404, 500, whatever */
  String * url;
  ArrayU8 * postdata;
  int postdata_offset;
  HTTP_STATE state;
} HTTPTransaction;


extern void HTTP_clear(HTTPTransaction ** t);

extern void HTTP_init_get(HTTPTransaction ** t, String * url);

extern void HTTP_init_put(HTTPTransaction ** t, String * url, ArrayU8 * postdata);

extern void HTTP_process(HTTPTransaction * t);

extern int HTTP_state(HTTPTransaction * t);


#endif
