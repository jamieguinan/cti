#include "SourceSink.h"
#include "Mem.h"
#include "Cfg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>		/* close */

typedef struct {
  FILE *f;			/* file */
  FILE *p;			/* pipe (popen) */
  int s;			/* socket */
  int state;			/* Relevant for sockets, may want to start with a POST. */
} Sink_private;


typedef struct {
  FILE *f;			/* file */
  long file_size;
  int s;			/* socket */
} Source_private;


Sink *Sink_new(char *name)
{
  int rc;
  Sink_private *priv = calloc(1, sizeof(*priv));

  priv->s = -1;			/* Default to invalid socket value. */

  char *colon = strchr(name, ':');
  if (colon && isdigit(colon[1])) {
    /* Split into host, port parts. */
    int hostlen = colon - name;
    char host[strlen(name)+1];
    char port[strlen(name)+1];
    strncpy(host, name, hostlen); host[hostlen] = 0;
    strcpy(port, colon+1);
    printf("host=%s port=%s\n", host, port);
    /* Connect to remote TCP port. */

    struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *results = 0L;

    rc = getaddrinfo(host, port,
			 &hints,
			 &results);
    if (rc != 0) {
      perror("getaddrinfo"); goto out;
    }

    priv->s = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    /* NOTE: Could be additional entries in linked list above. */

    if (priv->s == -1) {
      perror("socket"); goto out;
    }

    rc = connect(priv->s, results->ai_addr, results->ai_addrlen);
    if (rc == -1) {
      perror("connect"); 
      /* Note: alternatively, could keep the socket and retry the open... */
      close(priv->s);
      priv->s = -1;
      goto out;
    }

    freeaddrinfo(results);  results = 0L;
  }
  else { 
    /* Apply strftime() to allow time-based naming. */
    char out_name[256];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (!lt) {
      perror("localtime");
      goto out;
    }

    if (name[0] == '|') {
      /* Open as pipe. */
      strftime(out_name, sizeof(out_name), name, lt);
      priv->p = popen(out_name+1, "w");
    }
    else {
      /* Open as file. */
      strftime(out_name, sizeof(out_name), name, lt);
      priv->f = fopen(out_name, "wb");
    }
  }
  
 out:
  return (Sink*)priv;
}


void Sink_write(Sink *sink, void *data, int length)
{
  Sink_private *priv = (Sink_private *)sink;
  int n;

  if (priv->f) {
    n = fwrite(data, length, 1, priv->f);
    if (n != 1) {
      perror("fwrite");
    }
  }
  else if (priv->p) {
    n = fwrite(data, length, 1, priv->p);
    if (n != 1) {
      perror("fwrite");
    }
  }
  else if (priv->s != -1) {   
    int sent = 0;
    while (sent < length) {
      /* FIXME: This send() can block, which can cause the calling
	 Instance's Inputs to back up.  Not sure how to resolve this,
	 but should have some kind of solution or at least a
	 recommendation or note.  Maybe a timeout, and fill in an
	 error value if couldn't send all in time? */
      n = send(priv->s, ((char*)data)+sent, length, 0);
      if (n <= 0) {
	perror("send");
	break;
      }
      sent += n;
    }
  }
}


void Sink_close_current(Sink *sink)
{
}


void Sink_free(Sink **sink)
{
}


Source *Source_new(char *name)
{
  int rc;
  Source_private *priv = calloc(1, sizeof(*priv));

  priv->s = -1;			/* Default to invalid socket value. */

  char *colon = strchr(name, ':');
  if (colon && isdigit(colon[1])) {
    /* Split into host, port parts. */
    int hostlen = colon - name;
    char host[strlen(name)+1];
    char port[strlen(name)+1];
    strncpy(host, name, hostlen); host[hostlen] = 0;
    strcpy(port, colon+1);
    printf("host=%s port=%s\n", host, port);
    /* Connect to remote TCP port. */

    struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *results = 0L;

    rc = getaddrinfo(host, port,
			 &hints,
			 &results);
    if (rc != 0) {
      perror("getaddrinfo"); goto out;
    }

    priv->s = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    /* NOTE: Could be additional entries in linked list above. */

    if (priv->s == -1) {
      perror("socket"); goto out;
    }

    rc = connect(priv->s, results->ai_addr, results->ai_addrlen);
    if (rc == -1) {
      perror("connect"); 
      /* Note: alternatively, could keep the socket and retry the open... */
      close(priv->s);
      priv->s = -1;
      goto out;
    }

    freeaddrinfo(results);  results = 0L;
  }
  else {
    priv->f = fopen(name, "rb");
    if (!priv->f) {
      perror(name);
    }
    else {
      if (fseek(priv->f, 0, SEEK_END) == 0) {
	priv->file_size = ftell(priv->f);
	fseek(priv->f, 0, SEEK_SET);
      }
    }
  }
  
 out:
  return (Source*)priv;
}


ArrayU8 * Source_read(Source *source, int max_length)
{
  Source_private *priv = (Source_private *)source;

  if (priv->f) {
    uint8_t *tmp = Mem_calloc(1, max_length);
    int len = fread(tmp, 1, max_length, priv->f);
    if (len == 0) {
      perror("fread");
      Mem_free(tmp);
      return 0L;
    }

    if (len < max_length) {
      tmp = Mem_realloc(tmp, len);	/* Shorten! */
    }

    ArrayU8 *result = ArrayU8_new();
    ArrayU8_take_data(result, &tmp, len);
    return result;
  }
  else if (priv->s != -1) {
    uint8_t *tmp = Mem_calloc(1, max_length);
    int len = recv(priv->s, tmp, max_length, 0);
    if (len == 0) {
      fprintf(stderr, "recv(max_length=%d) -> %d\n", max_length, len);
      Mem_free(tmp);
      return 0L;
    }

    if (cfg.verbosity) {
      fprintf(stderr, "recv %d bytes\n", len);
    }

    if (len < max_length) {
      tmp = Mem_realloc(tmp, len);	/* Shorten! */
    }

    ArrayU8 *result = ArrayU8_new();
    ArrayU8_take_data(result, &tmp, len);
    return result;
  }
  return 0L;
}


int Source_seek(Source *source, long amount)
{
  Source_private *priv = (Source_private *)source;

  if (priv->f) {
    int rc = fseek(priv->f, amount, SEEK_CUR);
    long pos = ftell(priv->f);
    if (priv->file_size) {
      printf("offset %ld: %ld%%\n", pos, (pos*100)/priv->file_size);
    }
    
    return rc;
  }
  else if (priv->s != -1) {
    fprintf(stderr, "can't seek sockets!\n");
    return -1;
  }
  else {
    return -1;
  }
}


void Source_close_current(Source *source)
{
  Source_private *priv = (Source_private *)source;

  if (priv->f) {
    fclose(priv->f);
    priv->f = 0L;
  }
  else if (priv->s != -1) {   
    close(priv->s);
    priv->s = -1;
  }
}


void Source_free(Source **source)
{
  Source_close_current(*source);
  free(*source);
  *source = 0L;
}
