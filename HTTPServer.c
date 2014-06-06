/*
 * HTTP Server module.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* close */

#include "CTI.h"
#include "HTTPServer.h"
#include "SourceSink.h"
#include "socket_common.h"
#include "VirtualStorage.h"
//#include "Array.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input HTTPServer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output HTTPServer_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  listen_common lsc;		/* listen socket_common */
  VirtualStorage *vs;		/* virtual storage object */
} HTTPServer_private;


static int set_v4addr(Instance *pi, const char *value)
{
  fprintf(stderr, "%s:%s not implemented!\n", __FILE__, __func__);
  return 0;
}


static int set_v4port(Instance *pi, const char *value)
{
  /* NOTE: cannot cast int addresses to shorts in armeb, so don't
     use cti_set_*() for v4port */
  HTTPServer_private *priv = (HTTPServer_private *)pi;
  priv->lsc.port = atoi(value);
  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  HTTPServer_private *priv = (HTTPServer_private *)pi;
  return listen_socket_setup(&priv->lsc);
}


static int set_virtual_storage(Instance *pi, const char *value)
{
  HTTPServer_private *priv = (HTTPServer_private *)pi;
  Instance * i = InstanceGroup_find(gig, S((char*)value));
  if (i) {
    priv->vs = VirtualStorage_from_instance(pi);
  }
  return 0;
}



static Config config_table[] = {
  /* v4 ports only, maybe add v6 later... */
  { "v4addr", set_v4addr, 0L, 0L },
  { "v4port", set_v4port, 0L, 0L },

  { "enable", set_enable, 0L, 0L },
  { "virtual_storage",  set_virtual_storage, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

typedef struct {
  VirtualStorage *vs;
  int fd;
} VirtualStorage_and_FD;


static void * http_thread(void *data)
{
  VirtualStorage_and_FD *vsfd = data;
  VirtualStorage *vs = vsfd->vs;
  Mem_free(vsfd); vsfd = NULL;

  Comm comm = { .io.s = vsfd->fd, .io.state = IO_OPEN_SOCKET };
  ArrayU8 * request = ArrayU8_new();
  int n = 0;

  /* Strings that should be checked and freed on exit. */
  String * srq = String_value_none();
  String_list * request_lines = String_list_value_none();
  String_list * operation = String_list_value_none();

  /* Strings that are only references to other strings. */
  String * op = NULL;
  String * path = NULL;

  /* Read request... */
  while (1) {
    Comm_read_append_array(&comm, request);
    if (comm.io.state == IO_CLOSED) {
      fprintf(stderr, "%s: unexpected close\n", __func__);
      goto out;
    }
    n = ArrayU8_search(request, 0, ArrayU8_temp_string("\r\n\r\n"));
    if (n == -1) {
      /* Allow newlines without carriage returns, too. */
      n = ArrayU8_search(request, 0, ArrayU8_temp_string("\n\n"));
    }
    if (n > 0) {
      fprintf(stderr, "%s: found double newline at %d\n", __func__, n);
      break;
    }
  }

  /* Request should be a block of several lines of text. */
  srq = ArrayU8_to_string(&request);
  request_lines = String_split(srq, "\n");
  if (String_list_len(request_lines) > 0) {
    operation = String_split(String_list_get(request_lines, 0), " ");
    /* NOTE: Ignoring the remaining request lines for now. */
    if (String_list_len(operation) >= 2) {
      op = String_list_get(operation, 0);
      if (String_eq(op, S("GET"))) {
	printf("Looks like a GET request\n");
	path = String_list_get(operation, 1);      
      }
      else if (String_eq(op, S("POST"))) {
	printf("Looks like a POST request (not handled)\n");
      }
    }
  }

  if (!path) {
    /* FIXME: 400 Bad Request */
    goto out;
  }

  printf("path: %s\n", s(path));
  /* Try to find resource. */
  if (vs) {
    void * resource = VirtualStorage_get(vs, path);
  }
  
  /* If found, return 20x code and data. */
  /* Else return 40x code. */
  
  const char *msg = "fuck off\n";
  write(comm.io.s, msg, strlen(msg));

 out:
  Comm_close(&comm);

  if (!String_list_is_none(operation)) {
    String_list_free(&operation);
  }


  if (!String_list_is_none(request_lines)) {
    String_list_free(&request_lines);
  }

  if (!String_is_none(srq)) {
    String_free(&srq);
  }

  return NULL;
}

static void handle_client(VirtualStorage * vs, int fd)
{
  pthread_t thread;
  VirtualStorage_and_FD * vsfd = Mem_malloc(sizeof(*vsfd));
  vsfd->vs = vs;
  vsfd->fd = fd;
  pthread_create(&thread, NULL, http_thread, vsfd);
  pthread_detach(thread);
}

static void HTTPServer_tick(Instance *pi)
{
  HTTPServer_private *priv = (HTTPServer_private *)pi;

  Handler_message *hm;
  fd_set rfds;
  fd_set wfds;
  struct timeval tv;
  int maxfd = 0;
  int wait_flag = 1;

  if (priv->lsc.fd > 0) {
    /* Want to handle new and existing connections, so don't block in GetData. */
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_SET(priv->lsc.fd, &rfds);
  maxfd = cti_max(priv->lsc.fd, maxfd);

  tv.tv_sec = 0;
  tv.tv_usec = 1000  * 1;	/* ms */

  int n = select(maxfd+1, &rfds, &wfds, 0L, &tv);

  if (n == 0) {
    /* Nothing to do... */
    goto out;
  }

  if (FD_ISSET(priv->lsc.fd, &rfds)) {
    struct sockaddr_in addr;
    socklen_t addrlen;
    int fd = accept(priv->lsc.fd, (struct sockaddr *)&addr, &addrlen);
    if (fd == -1) {
      /* This is unlikely but possible.  If it happens, just clean up
	 and return... */
      perror("accept");
      goto out;
    }

    // fprintf(stderr, "accepted connection on port %d\n", priv->lsc.port);
    handle_client(priv->vs, fd);
  }

  out:
  pi->counter++;
}

static void HTTPServer_instance_init(Instance *pi)
{
  // HTTPServer_private *priv = (HTTPServer_private *)pi;
}


static Template HTTPServer_template = {
  .label = "HTTPServer",
  .priv_size = sizeof(HTTPServer_private),  
  .inputs = HTTPServer_inputs,
  .num_inputs = table_size(HTTPServer_inputs),
  .outputs = HTTPServer_outputs,
  .num_outputs = table_size(HTTPServer_outputs),
  .tick = HTTPServer_tick,
  .instance_init = HTTPServer_instance_init,
};

void HTTPServer_init(void)
{
  Template_register(&HTTPServer_template);
}
