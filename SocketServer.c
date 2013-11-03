/* 
 * This object receives RawData buffers, and distributes the data to
 * one or more connected clients.  If a client can't keep up, (that
 * is, too much data is buffered up), it gets disconnected.  It isn't
 * likely to scale past a relatively small number of clients, maybe 8
 * at the most.  Write another module if want to scale higher.
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

#ifdef __APPLE__
#define MSG_NOSIGNAL 0
#endif

/* FIXME: See arch-specific define over in modc code... */
#ifndef set_reuseaddr
#define set_reuseaddr 1
#endif

#define _max(a, b)  ((a) > (b) ? (a) : (b))
#define _min(a, b)  ((a) < (b) ? (a) : (b))

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
  char v4addr[4];
  unsigned short v4port;
  int listen_socket;
  Client_connection *cc_first;
  Client_connection *cc_last;
  RawData_node *raw_first;
  RawData_node *raw_last;
  int raw_seq;
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
  SocketServer_private *priv = (SocketServer_private *)pi;
  priv->v4port = atoi(value);
  return 0;
}

static int set_enable(Instance *pi, const char *value)
{
  SocketServer_private *priv = (SocketServer_private *)pi;
  int rc;

  priv->listen_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (priv->listen_socket == -1) {
    /* FIXME: see Socket.log_error in modc code */
    fprintf(stderr, "socket error\n");
    return 1;
  }

  struct sockaddr_in sa = { .sin_family = AF_INET,
			    .sin_port = htons(priv->v4port),
			    .sin_addr = { .s_addr = htonl(INADDR_ANY)}
  };

  if (set_reuseaddr) {
    int reuse = 1;
    rc = setsockopt(priv->listen_socket, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
    if (rc == -1) { 
      // Socket.log_error(s, "SO_REUSEADDR"); 
      fprintf(stderr, "SO_REUSEADDR\n"); 
      close(priv->listen_socket); priv->listen_socket = -1;
      return 1;
    }
  }

  rc = bind(priv->listen_socket, (struct sockaddr *)&sa, sizeof(sa));
  if (rc == -1) { 
    /* FIXME: see Socket.log_error in modc code */
    perror("bind"); 
    close(priv->listen_socket); priv->listen_socket = -1;
    return 1;
  }

  rc = listen(priv->listen_socket, 5);
  if (rc == -1) { 
    /* FIXME: see Socket.log_error in modc code */
    fprintf(stderr, "listen\n"); 
    close(priv->listen_socket); priv->listen_socket = -1;
    return 1;
  }

  printf("listening on port %d\n", priv->v4port);
  
  return 0;
}


static Config config_table[] = {
  { "max_total_buffered_data", set_max_total_buffered_data, 0L, 0L },
  { "v4addr", set_v4addr, 0L, 0L },
  { "v4port", set_v4port, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  /* Maybe add v6 later... */
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
  priv->raw_last->seq = priv->raw_seq;
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

  if (priv->listen_socket) {
    /* Want to handle new and existing connections, so don't block in GetData. */
    wait_flag = 0;
  }

  /* FIXME: Maybe should drain all input messages before going on the
     select() call.  If the frequency of incoming audio blocks exceeds
     the corresponding select() timeout frequency, the input queue will
     back up! */

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
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
      maxfd = _max(cc->fd, maxfd);
    }

    cc = cc->next;
  }

  FD_SET(priv->listen_socket, &rfds);
  maxfd = _max(priv->listen_socket, maxfd);

  tv.tv_sec = 0; tv.tv_usec = 1000;		/* 1ms */
  sn = select(maxfd+1, &rfds, &wfds, 0L, &tv);

  if (sn == 0) {
    /* Nothing to do... */
    goto out;
  }

  if (FD_ISSET(priv->listen_socket, &rfds)) {
    /* Add client connection. */
    cc = Mem_calloc(1, sizeof(*cc));
    cc->addrlen = sizeof(cc->addr);
    cc->t0 = time(NULL);
    cc->fd = accept(priv->listen_socket, (struct sockaddr *)&cc->addr, &cc->addrlen);
    if (cc->fd == -1) {
      /* This is unlikely but possible.  If it happens, just clean up
	 and return... */
      perror("accept");
      Mem_free(cc);
      goto out;
    }
    else {
      printf("accepted connection on port %d\n", priv->v4port);
    }

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

    to_send = _min(16384, cc->raw_node->buffer->data_length - cc->raw_offset);

    if (to_send <= 0) {
      fprintf(stderr, "Whoa, to_send is %d (cc->raw_node->buffer->data_length=%d cc->raw_offset=%d)\n", 
	      to_send, cc->raw_node->buffer->data_length, cc->raw_offset);
      n = 0;
      goto oops;
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
    dpf("soccketserver: %lld bytes/sec\n", 
	cc->bytes_sent / (time(NULL) - cc->t0));
    
  oops:
    cc->raw_offset += n;
    if (cc->raw_offset == cc->raw_node->buffer->data_length) {
      /* Move on to next node... */
      if (cc->raw_node->next) {
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
  
    RawData_buffer_discard(raw_tmp->buffer);
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

