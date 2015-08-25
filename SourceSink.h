#ifndef _SOURCESINK_H_
#define _SOURCESINK_H_

#include "ArrayU8.h"
#include "Stats.h"
#include <stdint.h>
#include <stdio.h>		/* FILE * */
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef enum {
  IO_CLOSED,
  IO_OPEN_FILE,
  IO_OPEN_PIPE,
  IO_OPEN_SOCKET,
} IO_state;

typedef struct {
  String * path;		/* name of file or socket path */
  String * generated_path;	/* strftime output */
  FILE *f;			/* file */
  FILE *p;			/* pipe (popen) */
  int s;			/* socket */
  struct sockaddr_in addr;	/* socket details... */
  socklen_t addrlen;		/* socket details... */
  IO_state state;
  long offset;
  unsigned int read_timeout_ms;
  ArrayU8 * extra;		/* extra data left over after certain operations */
} IO_common;

typedef struct {
  IO_common io;
  long file_size;
  int eof;
  int eof_flagged;
  int persist;			/* return on EOF */
  Items_per_sec bytes_per_sec;
} Source;

extern Source * Source_new(const char * path /* , options? */ );
extern Source * Source_allocate(const char * path);
extern ArrayU8 * Source_read(Source *source);
extern int Source_poll(Source *source, int timeout_ms);
extern int Source_seek(Source *source, long amount);
extern int Source_set_offset(Source *source, long amount);
extern long Source_tell(Source *source);
extern void Source_acquire_data(Source *source, ArrayU8 *chunk, int *needData);
extern void Source_set_persist(Source *source, int value);
extern void Source_close_current(Source *source);
extern void Source_reopen(Source *source);
extern void Source_free(Source **source);


typedef struct {
  IO_common io;
} Sink;

extern Sink * Sink_new(const char * path);
extern Sink * Sink_allocate(const char * path);
extern void Sink_write(Sink *sink, void *data, int length);
extern int Sink_poll(Sink *sink, int timeout_ms);
extern void Sink_flush(Sink *sink);
extern void Sink_close_current(Sink *sink);
extern void Sink_reopen(Sink *sink);
extern void Sink_free(Sink **sink);


typedef struct {
  /* 2-way socket or pipe. */
  IO_common io;
} Comm;

#define IO_ok(x)  (x->io.state != IO_CLOSED)

extern Comm * Comm_new(char * path);
extern void Comm_close(Comm * comm);
extern void Comm_write_string_with_byte(Comm * comm, String *str, char byteval);
extern String * Comm_read_string_to_byte(Comm * comm, char byteval);

extern void Comm_read_append_array(Comm * comm, ArrayU8 * array);
extern void Comm_write_from_array(Comm * comm,  ArrayU8 * array, unsigned long * offset);
extern void Comm_write_from_array_complete(Comm * comm,  ArrayU8 * array);
extern void Comm_write_from_string(Comm * comm,  String * str, unsigned long * offset);
extern void Comm_write_from_string_complete(Comm * comm,  String * str);
extern void Comm_free(Comm **comm);


#endif
