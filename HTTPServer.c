/*
 * HTTP Server module.
 * 
 * Normal usage is to associate a VirtualStorage instance, and
 * search that for requests.
 *
 */

/* Here are some examples from my local apache server, which I used as
   a reference while writing this code,

    HTTP/1.1 200 OK
    Date: Fri, 06 Jun 2014 20:34:32 GMT
    Server: Apache
    Last-Modified: Fri, 06 Jun 2014 20:34:15 GMT
    ETag: "be1dba-1500-4fb30c8efbfc0"
    Accept-Ranges: bytes
    Content-Length: 5376
    Content-Type: image/jpeg

    [data...]


    HTTP/1.1 400 Bad Request
    Date: Fri, 06 Jun 2014 20:38:03 GMT
    Server: Apache
    Content-Length: 285
    Connection: close
    Content-Type: text/html; charset=iso-8859-1

    <!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
    <html><head>
    <title>400 Bad Request</title>
    </head><body>
    <h1>Bad Request</h1>
    <p>Your browser sent a request that this server could not understand.<br />
    </p>
    <hr>
    <address>Apache Server at localhost Port 80</address>
    </body></html>


    HTTP/1.1 404 Not Found
    Date: Fri, 06 Jun 2014 20:36:44 GMT
    Server: Apache
    Content-Length: 267
    Content-Type: text/html; charset=iso-8859-1

    <!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
    <html><head>
    <title>404 Not Found</title>
    </head><body>
    <h1>Not Found</h1>
    <p>The requested URL /nosuch.jpg was not found on this server.</p>
    <hr>
    <address>Apache Server at localhost Port 80</address>
    </body></html>

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
//#include "ArrayU8.h"

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
    priv->vs = VirtualStorage_from_instance(i);
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
  /* This small structure is used to pass storage and file descriptor
     to HTTP handler thread. */
  VirtualStorage *vs;
  int fd;
} VirtualStorage_and_FD;


static void * http_thread(void *data)
{
  VirtualStorage_and_FD *vsfd = data;
  VirtualStorage *vs = vsfd->vs;
  Comm comm = { .io.s = vsfd->fd, .io.state = IO_OPEN_SOCKET };
  Mem_free(vsfd);
  ArrayU8 * request = ArrayU8_new();
  int n = 0;

  /* NOTE: This could handle HTTP/1.1 multiple items per connection by
     wrapping everything in a loop (or using "goto top") and not
     breaking until any of,
     (1) the client closes its end
     (2) the client sends invalid protocol
     (3) there are too many threads
     (4) N number of passes
  */

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
      // fprintf(stderr, "%s: found double newline at %d\n", __func__, n);
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
	// printf("Looks like a GET request\n");
	path = String_list_get(operation, 1);      
      }
      else if (String_eq(op, S("POST"))) {
	// printf("Looks like a POST request (not handled)\n");
      }
    }
  }

  if (!path) {
    /* Return 40x code. */
    static const char * text = 
      "<html>\n"
      "<head>\n"
      "<title>400 bad request</title>\n"
      "</head>\n"
      "<body>\n"
      "<p>bad request</p>\n"
      "</body>\n"
      "</html>\n"
      ;
    String *result = String_sprintf("HTTP/1.1 400 Bad Request\r\n"
				    // Date:
				    // Server:
				    // Last-modified:
				    // Etag:
				    // Accept-Ranges:
				    "Content-length: %zd\r\n"
				    "Content-type: %s\r\n"
				    "\r\n"
				    "%s",
				    strlen(text),
				    "text/html",
				    text
				    );
    Comm_write_from_string_complete(&comm, result);
    String_free(&result);
    goto out;
  }

  // printf("path: %s\n", s(path));

  /* Try to find resource. */
  Resource * resource = NULL;
  if (vs) {
    resource = VirtualStorage_get(vs, path);
  }

  if (resource) {
    /* Return 20x code, try sending. */
    do {
      String *hdr = String_sprintf("HTTP/1.1 200 OK\r\n"
				   // Date:
				   // Server:
				   // Last-modified:
				   // Etag:
				   // Accept-Ranges:
				   "Content-length: %ld\r\n"
				   "Content-type: %s\r\n"
				   "\r\n",
				   resource->size,
				   resource->mime_type
				   );

      Comm_write_from_string_complete(&comm, hdr);
      String_free(&hdr);
      if (comm.io.state == IO_CLOSED) { break; }

      ArrayU8 *ar = ArrayU8_temp_const(resource->data, resource->size);
      Comm_write_from_array_complete(&comm, ar);
      if (comm.io.state == IO_CLOSED) { break; }

    } while (0);
  }
  else {
    /* Return 40x code. */
    static const char * text = 
      "<html>\n"
      "<head>\n"
      "<title>404 path not found</title>\n"
      "</head>\n"
      "<body>\n"
      "<p>path not found</p>\n"
      "</body>\n"
      "</html>\n"
      ;
    String *result = String_sprintf("HTTP/1.1 404 Not Found\r\n"
				    // Date:
				    // Server:
				    // Last-modified:
				    // Etag:
				    // Accept-Ranges:
				    "Content-length: %zd\r\n"
				    "Content-type: %s\r\n"
				    "\r\n"
				    "%s",
				    strlen(text),
				    "text/html",
				    text
				    );
    Comm_write_from_string_complete(&comm, result);
    String_free(&result);
  }
  
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
    socklen_t addrlen = sizeof(addr);
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
