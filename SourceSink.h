#ifndef _SOURCESINK_H_
#define _SOURCESINK_H_

#include "Array.h"
#include <stdint.h>
#include <stdio.h>		/* FILE * */

typedef struct {
  FILE *f;			/* file */
  long file_size;
  int s;			/* socket */
  int eof;
  int eof_flagged;
  int persist;			/* return on EOF */
} Source;

extern Source *Source_new(char *name /* , options? */ );
extern ArrayU8 * Source_read(Source *source, int max_length);
extern int Source_seek(Source *source, long amount);
extern int Source_set_offset(Source *source, long amount);
extern long Source_tell(Source *source);
extern void Source_acquire_data(Source *source, ArrayU8 *chunk, int *needData);
extern void Source_set_persist(Source *source, int value);
extern void Source_close_current(Source *source);
extern void Source_free(Source **source);


typedef struct {
  /* All data is private.  Use only new and free functions below. */
} Sink;

extern Sink *Sink_new(char *name);
extern void Sink_write(Sink *sink, void *data, int length);
extern void Sink_close_current(Sink *sink);
extern void Sink_free(Sink **sink);
#endif
