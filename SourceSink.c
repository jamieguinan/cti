#include "SourceSink.h"
#include "Mem.h"
#include "Cfg.h"
#include "CTI.h"		/* dpf */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>		/* fstat */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>		/* close */

typedef struct {
  FILE *f;			/* file */
  FILE *p;			/* pipe (popen) */
  int s;			/* socket */
  int state;			/* Relevant for sockets, may want to start with a POST. */
} Sink_private;


Sink *Sink_new(char *name)
{
  int rc;
  Sink_private *priv = Mem_calloc(1, sizeof(*priv));

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
      fprintf(stderr, "fwrite wrote %d/%d bytes\n", n, length);
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
  Source *source = Mem_calloc(1, sizeof(*source));

  source->s = -1;			/* Default to invalid socket value. */

  source->t0 = time(NULL);

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

    source->s = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    /* NOTE: Could be additional entries in linked list above. */

    if (source->s == -1) {
      perror("socket"); goto out;
    }

    rc = connect(source->s, results->ai_addr, results->ai_addrlen);
    if (rc == -1) {
      perror("connect"); 
      /* Note: alternatively, could keep the socket and retry the open... */
      close(source->s);
      source->s = -1;
      goto out;
    }

    freeaddrinfo(results);  results = 0L;
  }
  else {
    /* Regular file. */
    source->persist = 1;
    source->f = fopen(name, "rb");
    if (!source->f) {
      perror(name);
    }
    else {
      if (fseek(source->f, 0, SEEK_END) == 0) {
	source->file_size = ftell(source->f);
	fseek(source->f, 0, SEEK_SET);
      }
    }
  }
  
 out:
  return source;
}


ArrayU8 * Source_read(Source *source, int max_length)
{
  if (source->f) {
    uint8_t *tmp = Mem_calloc(1, max_length);
    int len = fread(tmp, 1, max_length, source->f);
    if (len == 0) {
      //perror("fread");
      if (feof(source->f)) {
	//printf("EOF on file\n");
      }
      if (ferror(source->f)) {
	//printf("error on file\n");
      }
      clearerr(source->f);	/* Clear the EOF so that we can seek backwards. */
      if (feof(source->f)) {
	printf("EOF remains on file\n");
      }
      if (ferror(source->f)) {
	printf("error remains on file\n");
      }
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
  else if (source->s != -1) {
    uint8_t *tmp = Mem_calloc(1, max_length);
    int len = recv(source->s, tmp, max_length, 0);
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
  /* Seek relative to current position. */
  if (source->f) {
    int rc;
    struct stat st;
    long pos;

    fstat(fileno(source->f), &st);
    pos = ftell(source->f);
    
    /* Clamp seek to beginning and end of file. */
    if (pos + amount > st.st_size) {
      amount = st.st_size - pos;
    }
    else if (pos + amount < 0) {
      amount = - pos;
    }

    rc = fseek(source->f, amount, SEEK_CUR);
    if (rc != 0) {
      fprintf(stderr, "fseek(%ld) error, feof=%d ferror=%d ftell=%ld\n",
	      amount, feof(source->f), feof(source->f), ftell(source->f));
      perror("fseek");
    }
    pos = ftell(source->f);
    if (source->file_size) {
      printf("offset %ld: %ld%%\n", pos, (pos*100)/source->file_size);
    }

    return rc;
  }
  else if (source->s != -1) {
    fprintf(stderr, "can't seek sockets!\n");
    return -1;
  }
  else {
    return -1;
  }
}


int Source_set_offset(Source *source, long amount)
{
  if (source->f) {
    int rc = fseek(source->f, amount, SEEK_SET);
    long pos = ftell(source->f);
    if (source->file_size) {
      printf("offset %ld: %ld%%\n", pos, (pos*100)/source->file_size);
    }
    
    return rc;
  }
  else if (source->s != -1) {
    fprintf(stderr, "can't seek sockets!\n");
    return -1;
  }
  else {
    return -1;
  }
}


long Source_tell(Source *source)
{
  if (source->f) {
    return ftell(source->f);
  }
  else {
    /* Not a regular file. */
    return -1;
  }
}


void Source_close_current(Source *source)
{
  if (source->f) {
    fclose(source->f);
    source->f = 0L;
  }
  else if (source->s != -1) {   
    close(source->s);
    source->s = -1;
  }
}


void Source_free(Source **source)
{
  Source_close_current(*source);
  free(*source);
  *source = 0L;
}


void Source_acquire_data(Source *source, ArrayU8 *chunk, int *needData)
{
  {
    ArrayU8 *newChunk;
    /* Network reads can return short numbers if not much data is
       available, so using a size relatively large compared to an
       audio buffer or Jpeg frame should not cause real-time playback
       to suffer here. */
    newChunk = Source_read(source, 32768);
    if (!newChunk) {
      source->eof = 1;
      if (!source->eof_flagged) {
	fprintf(stderr, "%s: EOF on source\n", __func__);
	source->eof_flagged = 1;
      }
      // Source_close_current(source);
      return;
    }

    source->bytes_read += newChunk->len;
    dpf("source reading %d bytes/sec\n", source->bytes_read / (time(NULL) - (source->t0)));


    if (source->eof_flagged) {
      fprintf(stderr, "%s: EOF reset\n", __func__);      
    }
    source->eof = 0;
    source->eof_flagged = 0;
    ArrayU8_append(chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    if (cfg.verbosity) { fprintf(stderr, "needData = 0\n"); }
    *needData = 0;
  }
}


void Source_set_persist(Source *source, int value)
{
  source->persist = value;  
}
