#include "SourceSink.h"
#include "Mem.h"
#include "String.h"
#include "dpf.h"
//#include "CTI.h"		/* dpf */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>		/* fstat */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>		/* close, fcntl */
#include <poll.h>		/* poll */
#include <fcntl.h>		/* fcntl */
#include <errno.h>


static void io_close_current(IO_common *io)
{
  if (io->state == IO_CLOSED) {
    return;
  }

  if (io->f) {
    fclose(io->f);
    io->f = NULL;
  }
  else if (io->p) {
    pclose(io->p);
    io->p = NULL;
  }
  else if (io->s != -1) {   
    close(io->s);
    io->s = -1;
  }
  io->state = IO_CLOSED;
}


static void io_open(IO_common *io, const char *mode)
{
  int rc;
  struct addrinfo *results = 0L;

  io->s = -1;			/* Default to invalid socket value. */
  io->state = IO_CLOSED;

  char * path = s(io->path);	/* convenience */

  char * colon = strchr(path, ':');
  if (colon && isdigit(colon[1])) {
    /* Split into host, port parts. */
    int hostlen = colon - path;
    char host[strlen(path)+1];
    char port[strlen(path)+1];
    strncpy(host, path, hostlen); host[hostlen] = 0;
    strcpy(port, colon+1);
    // fprintf(stderr, "host=%s port=%s\n", host, port);
    /* Connect to remote TCP port. */
    struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *rp = 0L;
    rc = getaddrinfo(host, port,
			 &hints,
			 &results);
    if (rc != 0) {
      perror("getaddrinfo"); goto out;
    }

    for (rp=results; rp != NULL; rp = rp->ai_next) {
      io->s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (io->s == -1) {
        perror("socket"); continue;
      }
  #if 0
      if (fcntl(io->s, FD_CLOEXEC)) {
        perror("FD_CLOEXEC");
      }
  #endif
      rc = connect(io->s, rp->ai_addr, rp->ai_addrlen);
      //fprintf(stderr, "rc=%d io->s=%d family=%d socktype=%d protocol=%d addrlen=%d, errno=%d\n", 
      //  rc, io->s, rp->ai_family, rp->ai_socktype, rp->ai_protocol, rp->ai_addrlen, errno);
      if (rc == 0) {
        fprintf(stderr, "connect Ok\n");
	break;
      }
      if (rc == -1) {
        perror("connect");
        /* Note: alternatively, could keep the socket and retry the open... */
        close(io->s);
        io->s = -1;
        io->state = IO_CLOSED;
      }
    }
    if (io->s == -1) {
      goto out;
    }
    io->state = IO_OPEN_SOCKET;
  }
  else {
    /* File.  Apply strftime() to allow time-based naming. */
    char out_path[256];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (!lt) {
      perror("localtime");
      goto out;
    }

    if (path[0] == '|' && mode[0] == 'w') {
      /* Open as pipe.  I noted at one point this worked only for
	 output.  Don't remember why, but I'm keeping the 'w' test
	 above just in case. */
      strftime(out_path, sizeof(out_path), path, lt);
      io->p = popen(out_path+1, "w");
      if (io->p) {
	io->state = IO_OPEN_PIPE;
	String_set(&io->generated_path, out_path);
      }
    }
    else {
      /* Open as regular file. */
      strftime(out_path, sizeof(out_path), path, lt);
      io->f = fopen(out_path, mode);
      if (io->f) {
	io->state = IO_OPEN_FILE;
	String_set(&io->generated_path, out_path);
      }
    }
  }

 out:
  if (results) {
    freeaddrinfo(results);  results = 0L;
  }

  return;
}


static void io_write(IO_common * io, void *data, int length)
{
  int n;

  if (io->f) {
    n = fwrite(data, 1, length, io->f);
    if (n != length) {
      fprintf(stderr, "fwrite wrote %d/%d bytes\n", n, length);
      io_close_current(io);
    }
  }
  else if (io->p) {
    n = fwrite(data, 1, length, io->p);
    if (n != length) {
      fprintf(stderr, "fwrite wrote %d/%d bytes\n", n, length);
      io_close_current(io);
    }
  }
  else if (io->s != -1) {   
    int sent = 0;
    while (sent < length) {
      /* FIXME: This send() can block, which can cause the calling
	 Instance's Inputs to back up.  Not sure how to resolve this,
	 but should have some kind of solution or at least a
	 recommendation or note.  Maybe a timeout, and fill in an
	 error value if couldn't send all in time? */
      //if (io_writable(io)) {
      //}
      n = send(io->s, ((char*)data)+sent, length, 0);
      if (n <= 0) {
	perror("send");
	break;
      }
      sent += n;
    }
  }
}



Sink * Sink_allocate(const char * path)
{
  /* Allocate but do not open. */
  Sink *sink = Mem_calloc(1, sizeof(*sink));
  String_set(&sink->io.path, path);
  sink->io.state = IO_CLOSED;
  return sink;
}


Sink *Sink_new(const char * path)
{
  Sink *sink = Mem_calloc(1, sizeof(*sink));
  String_set(&sink->io.path, path);
  io_open(&sink->io, "wb");
  return sink;
}


int Sink_poll(Sink *sink, int timeout_ms)
{
  struct pollfd fds[1];
  fds[0].events = POLLOUT;
  if (sink->io.f) {
    fds[0].fd = fileno(sink->io.f);
  }
  else if (sink->io.s != -1) {
    fds[0].fd = sink->io.s;
  }
  else {
    return 0;
  }

  return poll(fds, 1, timeout_ms);
}


void Sink_write(Sink *sink, void *data, int length)
{
  io_write(&sink->io, data, length);
}


void Sink_flush(Sink *sink)
{
  if (sink->io.f) {
    fflush(sink->io.f);
  }
  else if (sink->io.p) {
    fflush(sink->io.p);
  }
}


void Sink_close_current(Sink *sink)
{
  io_close_current(&sink->io);
}


void Sink_reopen(Sink *sink)
{
  if (!sink) {
    fprintf(stderr, "%s: NULL sink\n", __func__);
    return;
  }
  io_open(&sink->io, "wb");
}


void Sink_free(Sink **sink)
{
  Sink_close_current(*sink);
  Mem_free(*sink);
  *sink = NULL;
}


Source * Source_allocate(const char * path)
{
  Source * source = Mem_calloc(1, sizeof(*source));
  String_set(&source->io.path, path);  
  source->io.state = IO_CLOSED;
  return source;
}


Source * Source_new(const char * path)
{
  Source * source = Mem_calloc(1, sizeof(*source));

  String_set(&source->io.path, path);
  io_open(&source->io, "rb");

  if (source->io.state == IO_OPEN_FILE) {
    if (fseek(source->io.f, 0, SEEK_END) == 0) {
      source->file_size = ftell(source->io.f);
      fseek(source->io.f, 0, SEEK_SET);
    }
  }
  return source;
}


ArrayU8 * io_read(IO_common *io)
{
  /* FIXME: Allow and handle timeouts. */
  int max_read = 32000;
  uint8_t * tmp = Mem_calloc(1, max_read);
  ArrayU8 * result = NULL;
  int len = 0;

  /* First copy from io->extra. */
  if (io->extra) {
    result = io->extra; io->extra = NULL;
  }
  else {
    result = ArrayU8_new();
  }

  if (io->f) {
    len = fread(tmp, 1, max_read, io->f);
    if (len == 0) {
      //perror("fread");
      if (feof(io->f)) {
	//printf("EOF on file\n");
      }
      if (ferror(io->f)) {
	//printf("error on file\n");
      }
      clearerr(io->f);	/* Clear the EOF so that we can seek backwards. */
      if (feof(io->f)) {
	printf("EOF remains on file\n");
      }
      if (ferror(io->f)) {
	printf("error remains on file\n");
      }
    }
  }
  else if (io->p) {
    len = fread(tmp, 1, max_read, io->p);
    if (len == 0) {
      //perror("fread");
      if (feof(io->p)) {
	//printf("EOF on file\n");
      }
      if (ferror(io->p)) {
	//printf("error on file\n");
      }
      clearerr(io->p);	/* Clear the EOF so that we can seek backwards. */
      if (feof(io->p)) {
	printf("EOF remains on file\n");
      }
      if (ferror(io->p)) {
	printf("error remains on file\n");
      }
    }
  }
  else if (io->s != -1) {
    len = recv(io->s, tmp, max_read, 0);
    if (len == 0) {
      //fprintf(stderr, "recv(max_read=%d) -> %d\n", max_read, len);
      io_close_current(io);
    }

    dpf("recv %d bytes (max_read=%d)\n", len, max_read);
  }
  
  if (len == 0) {
    io->extra = result;
    result = 0L;
  }
  else if (len == -1) {
    fprintf(stderr, "read returned -1 (socket read?)\n");
    io->extra = result;
    result = 0L;
  }
  else {
    ArrayU8_append(result, ArrayU8_temp_const(tmp, len));
  }

  Mem_free(tmp);

  return result;
}


ArrayU8 * Source_read(Source *source)
{
  return io_read(&source->io);
}


int Source_poll(Source *source, int timeout_ms)
{
  struct pollfd fds[1];
  fds[0].events = POLLIN;
  
  if (source->io.f) {
    fds[0].fd = fileno(source->io.f);
  }
  else if (source->io.s != -1) {
    fds[0].fd = source->io.s;
  }
  else {
    return 0;
  }
  
  return poll(fds, 1, timeout_ms);
}


int Source_seek(Source *source, long amount)
{
  /* Seek relative to current position. */
  if (source->io.f) {
    int rc;
    struct stat st;
    long pos;

    fstat(fileno(source->io.f), &st);
    pos = ftell(source->io.f);
    
    /* Clamp seek to beginning and end of file. */
    if (pos + amount > st.st_size) {
      amount = st.st_size - pos;
    }
    else if (pos + amount < 0) {
      amount = - pos;
    }

    rc = fseek(source->io.f, amount, SEEK_CUR);
    if (rc != 0) {
      fprintf(stderr, "fseek(%ld) error, feof=%d ferror=%d ftell=%ld\n",
	      amount, feof(source->io.f), feof(source->io.f), ftell(source->io.f));
      perror("fseek");
    }
    pos = ftell(source->io.f);
    if (source->file_size) {
      printf("offset %ld: %ld%%\n", pos, (pos*100)/source->file_size);
    }

    return rc;
  }
  else if (source->io.s != -1) {
    fprintf(stderr, "can't seek sockets!\n");
    return -1;
  }
  else {
    return -1;
  }
}


int Source_set_offset(Source *source, long amount)
{
  if (source->io.f) {
    int rc = fseek(source->io.f, amount, SEEK_SET);
    long pos = ftell(source->io.f);
    if (source->file_size) {
      printf("offset %ld: %ld%%\n", pos, (pos*100)/source->file_size);
    }
    
    return rc;
  }
  else if (source->io.s != -1) {
    fprintf(stderr, "can't seek sockets!\n");
    return -1;
  }
  else {
    return -1;
  }
}


long Source_tell(Source *source)
{
  if (source->io.f) {
    return ftell(source->io.f);
  }
  else {
    /* Not a regular file. */
    return -1;
  }
}


void Source_close_current(Source *source)
{
  if (source->io.f) {
    fclose(source->io.f);
    source->io.f = 0L;
  }
  else if (source->io.s != -1) {   
    close(source->io.s);
    source->io.s = -1;
  }
  source->io.state = IO_CLOSED;
}


void Source_reopen(Source * source)
{
  if (!source) {
    fprintf(stderr, "%s: NULL source\n", __func__);
    return;
  }
  io_open(&source->io, "rb");
}


void Source_free(Source **source)
{
  Source_close_current(*source);
  Mem_free(*source);
  *source = 0L;
}

extern void StackDebug(void);

void Source_acquire_data(Source *source, ArrayU8 *chunk, int *needData)
{
  {
    ArrayU8 *newChunk;
    /* Network reads can return short numbers if not much data is
       available, so using a size relatively large compared to an
       audio buffer or Jpeg frame should not cause real-time playback
       to suffer here. */
    newChunk = Source_read(source);
    if (!newChunk) {
      source->eof = 1;
      if (!source->eof_flagged) {
	fprintf(stderr, "%s: EOF on source\n", __func__);
	source->eof_flagged = 1;
	/* Adding a short sleep here will tend to sort things out, when a player
	   is trying to keep up with a live stream and hits the end. */
	nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/50}, NULL);
      }
      // Source_close_current(source);
      return;
    }

    Items_per_sec_update(&source->bytes_per_sec, newChunk->len);

    { 
      static time_t t_last = 0;
      time_t tnow = time(NULL);
      if (tnow > t_last) {
	dpf("source bytes/sec %.1f %.1f %1.f\n",
	    source->bytes_per_sec.records[0].value,
	    source->bytes_per_sec.records[1].value,
	    source->bytes_per_sec.records[2].value);
	t_last = tnow;
      }
    }

    if (source->eof_flagged) {
      fprintf(stderr, "%s: EOF reset\n", __func__);      
    }
    source->eof = 0;
    source->eof_flagged = 0;
    ArrayU8_append(chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    dpf("needData = 0\n", NULL);
    *needData = 0;
  }
}


void Source_set_persist(Source *source, int value)
{
  source->persist = value;  
}


/* New 2-way Comm interface. */
Comm * Comm_new(char * path)
{
  Comm * comm = Mem_calloc(1, sizeof(*comm));

  comm->io.s = -1;			/* Default to invalid socket value. */
  comm->io.state = IO_CLOSED;
  String_set(&comm->io.path, path);
  io_open(&comm->io, "w+b");
  
  return comm;
}


String * Comm_read_string_to_byte(Comm * comm, char byteval)
{
  ArrayU8 * chunk = NULL;
  int i;

  /* Read up to indicated byte value, return a String, store extra in io.extra. */
  while (1) {
    ArrayU8 * new_chunk = io_read(&comm->io);
    if (!new_chunk) {
      /* FIXME: Clean up */
      fprintf(stderr, "new_chunk NULL\n");
      return String_value_none();
    }

    // fprintf(stderr, "new_chunk len %d\n", new_chunk->len);
    
    if (!chunk) {
      chunk = new_chunk;
    }
    else {
      ArrayU8_append(chunk, new_chunk);
      /* FIXME: Free new_chunk */
    }
    
    //fprintf(stderr, "scanning %d bytes for value 0x%x\n", chunk->len, byteval);
    
    for (i=0; i < chunk->len; i++) {
      //fprintf(stderr, "chunk->data[i] = 0x%x [%c]\n", chunk->data[i], chunk->data[i]);
      if (chunk->data[i] == byteval) {
	//fprintf(stderr, "found at offset %d\n", i);	
	String *result = String_new("");
	String_append_bytes(result, (char*)chunk->data, i);
	if (chunk->len > i && !comm->io.extra) {
	  comm->io.extra = ArrayU8_new();
	  ArrayU8_append(comm->io.extra, ArrayU8_temp_const(chunk->data+i, chunk->len-i));
	}
	ArrayU8_free(&chunk);
	return result;
      }
    }
    
  }
  return String_value_none();
}

void Comm_close(Comm * comm)
{
  io_close_current(&comm->io);
}


void Comm_read_append_array(Comm * comm, ArrayU8 * array)
{
  /* FIXME: This would be more efficient if io_read read straight into
     the array, rather than allocate/append/free. */
  ArrayU8 * chunk = io_read(&comm->io);
  if (chunk) {
    ArrayU8_append(array, chunk);
    ArrayU8_free(&chunk);
  }
}


void Comm_write_from_array(Comm * comm,  ArrayU8 * array, unsigned long * offset)
{
  /* FIXME: Update offset? */
  io_write(&comm->io, array->data, array->len);
}


void Comm_write_from_array_complete(Comm * comm,  ArrayU8 * array)
{
  /* FIXME: Repeat until finished? */
  io_write(&comm->io, array->data, array->len);
}


void Comm_write_from_string(Comm * comm,  String * str, unsigned long * offset)
{
  /* FIXME: Update offset? */
  io_write(&comm->io, str->bytes, str->len);
}


void Comm_write_from_string_complete(Comm * comm, String * str)
{
  /* FIXME: Repeat until finished? */
  io_write(&comm->io, str->bytes, str->len);
}


void Comm_write_string_with_byte(Comm * comm, String *str, char byteval)
{
  /* FIXME: Could make this a single call if implemented io_writev. */
  io_write(&comm->io, s(str), String_len(str));
  io_write(&comm->io, &byteval, 1);
}


void Comm_free(Comm **comm)
{
  if (comm && *comm) {
    Comm_close(*comm);
    Mem_free(*comm);
    *comm = 0L;
  }
}


