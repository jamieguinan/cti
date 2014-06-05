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



static Config config_table[] = {
  /* v4 ports only, maybe add v6 later... */
  { "v4addr", set_v4addr, 0L, 0L },
  { "v4port", set_v4port, 0L, 0L },

  { "enable", set_enable, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void * http_thread(void *data)
{
  int fd = (long)data;
  // Comm comm = { .io.s = fd };

  /* Read request... */
  /* Try to find resource. */
  /* If found, return 20x code and data. */
  /* Else if not found, return 404 code. */
  /* Use Comm_read and Comm_write, fill in that code as necessary. */

  const char *msg = "fuck off\n";
  write(fd, msg, strlen(msg));
  close(fd);
  return NULL;
}

static void handle_client(int fd)
{
  pthread_t thread;
  pthread_create(&thread, NULL, http_thread, (void*)(long)fd);
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
    handle_client(fd);
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
