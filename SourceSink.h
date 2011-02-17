#ifndef _SOURCESINK_H_
#define _SOURCESINK_H_

#include "Array.h"
#include <stdint.h>

typedef struct {
  /* All data is private.  Use only new and free functions below. */
} Source;

extern Source *Source_new(char *name /* , options? */ );
extern ArrayU8 * Source_read(Source *source, int max_length);
extern int Source_seek(Source *source, long amount);
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
