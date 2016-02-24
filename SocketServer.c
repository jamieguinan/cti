/* 
 * SocketServer supports this mode of operation: receive RawData
 * messages, keep them in a ring buffer, and send the data to all
 * connected clients.  If a client can't keep up, (that is, too much
 * data is buffered up), it gets disconnected.
 *
 * This module isn't likely to scale past a relatively small number
 * of clients, maybe 8 at the most.
 *
 * It is also single-threaded, which lets it avoid locking the data
 * ring buffer for client access.
 *
 * Am also considering using this as a message listener and protocol
 * handler.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include <sys/select.h>		/* select, POSIX (see man page) */
#include <sys/socket.h>		/* shutdown */
#include <unistd.h>		/* close */

#include <netinet/in.h>
#include <arpa/inet.h>

#include "CTI.h"
#include "SocketServer.h"
#include "Images.h"
#include "socket_common.h"

#ifdef __APPLE__
#define MSG_NOSIGNAL 0
#endif

static void Config_handler(Instance *pi, void *data);
static void RawData_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_RAWDATA };
static Input SocketServer_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_buffer", .handler = Config_handler },
  [ INPUT_RAWDATA ] = { .type_label = "RawData_buffer", .handler = RawData_handler },
};

enum { OUTPUT_RAWDATA };
static Output SocketServer_outputs[] = {
  [ OUTPUT_RAWDATA ] = { .type_label = "RawData_buffer", .destination = 0L },
};

typedef struct _Client_connection {
  struct _Client_connection *prev;
  struct _Client_connection *next;
  struct sockaddr_in addr;
  socklen_t addrlen;
  int fd;
  int state;
  int raw_seq;			/* sequence number in node-being-transmitted */
  RawData_node *raw_node;	/* node-being-transmitted */
  int raw_offset;
  uint64_t bytes_sent;
  time_t t0;
} Client_connection;

typedef enum {
  CC_INIT,
  CC_READY_TO_SEND,
  CC_WAITING_FOR_NODE,
  CC_CLOSEME,
} ClientStates;


typedef struct {
  Instance i;
  int max_total_buffered_data;	/* Start dropping after this is exceeded. */
  int total_buffered_data;
  listen_common lsc;		/* listen socket_common */
  Client_connection *cc_first;
  Client_connection *cc_last;
  int num_client_connections;
  RawData_node *raw_first;
  RawData_node *raw_last;
  int raw_seq;
  ISet(Instance) notify_instances; 
} SocketServer_private;


static int set_max_total_buffered_data(Instance *pi, const char *value)
{
  SocketServer_private *priv = (SocketServer_private *)pi;
  priv->max_total_buffered_data = atoi(value);
  return 0;
}


static int set_v4addr(Instance *pi, const char *value)
{
  fprintf(stderr, "%s:%s not implemented!\n", __FILE__, __func__);
  return 0;
}


static int set_v4port(Instance *pi, const char *value)
{
  /* NOTE: cannot cast int addresses to shorts in armeb, so don't
     use cti_set_*() for v4port */
  SocketServer_private *priv = (SocketServer_private *)pi;
  priv->lsc.port = atoi(value);
  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  SocketServer_private *priv = (SocketServer_private *)pi;
  return listen_socket_setup(&priv->lsc);
}


static int set_notify(Instance *pi, const char *value)
{
  /* Add to list of clients that get a notify message when connection
     count transtions to/from zero. */
  SocketServer_private *priv = (SocketServer_private *)pi;

  Instance *px = InstanceGroup_find(gig, S((char*)value));

  if (!px) {
    return 1;
  }

  ISet_add(priv->notify_instances, px);
  
  return 0;
}



static Config config_table[] = {
  { "max_total_buffered_data", set_max_total_buffered_data, 0L, 0L },

  /* v4 ports only, maybe add v6 later... */
  { "v4addr", set_v4addr, 0L, 0L },
  { "v4port", set_v4port, 0L, 0L },

  { "enable", set_enable, 0L, 0L },
  { "notify", set_notify, 0L, 0L},
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void RawData_handler(Instance *pi, void *data)
{
  SocketServer_private *priv = (SocketServer_private *)pi;
  RawData_buffer *raw = data;
  RawData_node *rn = Mem_calloc(1, sizeof(*rn));
  rn->buffer = raw;

  if (!priv->raw_first) {
    priv->raw_first = priv->raw_last = rn;
  }
  else {
    priv->raw_last->next = rn;
    priv->raw_last = rn;
  }

  priv->total_buffered_data += priv->raw_last->buffer->data_length;

  priv->raw_seq += 1;
  rn->seq = priv->raw_seq;
}


static void toggle_services(SocketServer_private *priv)
{
  /* This is called whenever priv->num_client_connections changes, so
     the value has just been transitioned to. */
  int i;
  printf("toggle_services() priv->num_client_connections=%d\n", priv->num_client_connections);

  if (priv->num_client_connections == 0) {
    /* Turn off. */
    for (i=0; i < priv->notify_instances.count; i++) {
      Instance *px = priv->notify_instances.items[i];
      printf("enable 0 to %s\n", px->label);
      PostData(Config_buffer_new("enable", "0"), &px->inputs[0]);
    }
  }
  else if (priv->num_client_connections == 1) {
    /* Turn on. */    
    for (i=0; i < priv->notify_instances.count; i++) {
      Instance *px = priv->notify_instances.items[i];
      printf("enable 1 to %s\n", px->label);
      PostData(Config_buffer_new("enable", "1"), &px->inputs[0]);
    }
  }
}


static void SocketServer_tick(Instance *pi)
{
  SocketServer_private *priv = (SocketServer_private *)pi;
  Handler_message *hm;
  Client_connection *cc;
  int wait_flag = 1;
  fd_set rfds;
  fd_set wfds;
  int maxfd = 0;
  int sn;
  struct timeval tv;

  if (priv->lsc.fd > 0) {
    /* Want to handle new and existing connections, so don't block in GetData. */
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  /* Assemble a select() call and flag each client connection writable
     or not.  Can add a small timeout value there to avoid spinning
     the CPU, allowing to return and call GetData above in a
     reasonable amount of time. */
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  cc = priv->cc_first; 

  while (cc) {
    if (cc->state == CC_CLOSEME) {
      Client_connection *cc_tmp;
      printf("closing fd=%d\n", cc->fd);
      shutdown(cc->fd, SHUT_RDWR);
      close(cc->fd);

      /* Delete from list.  Note, single node will have been at
	 beginning and end, result will be empty cc_first and
	 cc_last. */
      if (cc->prev) {
	cc->prev->next = cc->next;
      } 
      else {
	/* Was at beginning of list. */
	priv->cc_first = cc->next;
      }

      if (cc->next) {
	cc->next->prev = cc->prev;
      }      
      else {
	/* Was at end of list. */
	priv->cc_last = cc->prev;
      }

      cc_tmp = cc->next;
      Mem_free(cc);
      cc = cc_tmp; 
      priv->num_client_connections -= 1;
      toggle_services(priv);
      continue;
    }

    if (cc->state == CC_INIT) {
      if (priv->raw_last) {
	/* Always start at the end, client can figure out buffering... */
	cc->raw_node = priv->raw_last;
	cc->raw_seq = cc->raw_node->seq;
	cc->raw_offset = 0;
	cc->state = CC_READY_TO_SEND;
      }
    }

    if (cc->state == CC_WAITING_FOR_NODE) {
      if (cc->raw_node->next) {
	cc->raw_node = cc->raw_node->next;
	cc->raw_seq = cc->raw_node->seq;
	cc->raw_offset = 0;
	cc->state = CC_READY_TO_SEND;
      }
      else {
	/* State remains CC_WAITING_FOR_NODE, will not call FD_SET for this node. */
      }
    }

    if (cc->state == CC_READY_TO_SEND) {
      FD_SET(cc->fd, &wfds);
      maxfd = cti_max(cc->fd, maxfd);
    }

    cc = cc->next;
  }

  FD_SET(priv->lsc.fd, &rfds);
  maxfd = cti_max(priv->lsc.fd, maxfd);

  /* The select() timeout here has been tuned for CTI apps that have
     multiple SocketServers receiving data from a Splitter.  If the
     value is too long, and only one of the SocketServers is active,
     the inputs to the other SocketServer will back up over several
     seconds, and then the whole chain will back up like this,

       CJpeg(cj) <- Splitter(s) <- MjpegMux(mjm2) <- SocketServer(ss2)

     On the other hand, the SocketServers tend to eat CPU, so setting
     it to 1ms can be expensive.  If CTI were a "pull" design instead
     of "push", this might not have become a concern, but for now CTI
     is "push" and that works well enough for my purposes.
     
     Update: reduced from 3ms to 1ms, because the large number of
     short audio samples was backing up the queues on the birdcam
     installation.
  */
  tv.tv_sec = 0;
  tv.tv_usec = 1000  * 1;	/* ms */

  sn = select(maxfd+1, &rfds, &wfds, 0L, &tv);

  if (sn == 0) {
    /* Nothing to do... */
    goto out;
  }

  if (FD_ISSET(priv->lsc.fd, &rfds)) {
    /* Add client connection. */
    cc = Mem_calloc(1, sizeof(*cc));
    cc->addrlen = sizeof(cc->addr);
    cc->t0 = time(NULL);
    cc->fd = accept(priv->lsc.fd, (struct sockaddr *)&cc->addr, &cc->addrlen);
    if (cc->fd == -1) {
      /* This is unlikely but possible.  If it happens, just clean up
	 and return... */
      perror("accept");
      Mem_free(cc);
      goto out;
    }

    fprintf(stderr, "accepted connection on port %d\n", priv->lsc.port);

    priv->num_client_connections += 1;
    toggle_services(priv);

    cc->state = CC_INIT;

    if (!priv->cc_first) {
      priv->cc_first = priv->cc_last = cc;
    }
    else {
      /* Add to end of list. */
      priv->cc_last->next = cc;
      cc->prev = priv->cc_last;
      priv->cc_last = cc;
    }
  }

  /* Iterate through client connection list, send some data. */
  for (cc = priv->cc_first; cc; cc = cc->next) {
    int n;
    int to_send;

    if (!FD_ISSET(cc->fd, &wfds)) {
      continue;
    }

    if (cc->raw_seq < priv->raw_first->seq) {
      /* Client fell behind, data is gone. */
      fprintf(stderr, "client fell behind...\n");
      cc->state = CC_CLOSEME;
      continue;
    }

    to_send = cti_min(16384, cc->raw_node->buffer->data_length - cc->raw_offset);

    if (to_send <= 0) {
      fprintf(stderr, "Whoa, to_send is %d (cc->raw_node->buffer->data_length=%d cc->raw_offset=%d)\n", 
	      to_send, cc->raw_node->buffer->data_length, cc->raw_offset);
      n = 0;
      goto nextraw;
    }

    n = send(cc->fd, cc->raw_node->buffer->data + cc->raw_offset, to_send, MSG_NOSIGNAL);
    if (n < 0) {
      perror("send");
      cc->state = CC_CLOSEME;
      continue;
    }

    if (n == 0) {
      fprintf(stderr, "send(%d) wrote %d bytes\n", to_send, n);
    }

    cc->bytes_sent += n;

  nextraw:

    cc->raw_offset += n;
    if (cc->raw_offset == cc->raw_node->buffer->data_length) {
      /* Finished sending current raw node. */
      if (cc->raw_node->next) {
	/* Move on to next node... */
	cc->raw_node = cc->raw_node->next;
	cc->raw_seq = cc->raw_node->seq;
	cc->raw_offset = 0;
      }
      else {
	/* Or flag to wait for more nodes. */
	cc->state = CC_WAITING_FOR_NODE;
      }
    }
  }


 out:

  /* Delete raw data nodes if buffering limit exceeded. */
  // printf("%d/%d\n", priv->total_buffered_data, priv->max_total_buffered_data);
  while (priv->total_buffered_data > priv->max_total_buffered_data) {
    RawData_node *raw_tmp = priv->raw_first;
    priv->raw_first = priv->raw_first->next;
    if (priv->raw_first == 0L) {
      priv->raw_last = 0L;	/* for consistency */
      fprintf(stderr, "%s:%s: BAD: no nodes left.  Too large buffer, or too small max?\n", __FILE__, __func__);
      while (1) { sleep (1); }
    }
    
    priv->total_buffered_data -= raw_tmp->buffer->data_length;
    // printf("* priv->total_buffered_data=%d\n", priv->total_buffered_data);
  
    RawData_buffer_release(raw_tmp->buffer);
    Mem_free(raw_tmp);
  }
}

static void SocketServer_instance_init(Instance *pi)
{
  SocketServer_private *priv = (SocketServer_private *)pi;

   /* Default to 2MB buffered data. */
  priv->max_total_buffered_data = 2*1024*1024;

  
}


static Template SocketServer_template = {
  .label = "SocketServer",
  .priv_size = sizeof(SocketServer_private),
  .inputs = SocketServer_inputs,
  .num_inputs = table_size(SocketServer_inputs),
  .outputs = SocketServer_outputs,
  .num_outputs = table_size(SocketServer_outputs),
  .tick = SocketServer_tick,
  .instance_init = SocketServer_instance_init,
};

void SocketServer_init(void)
{
  Template_register(&SocketServer_template);
}

