#ifndef _CTI_ARRAY_H_
#define _CTI_ARRAY_H_

#include <stdint.h>
#include "String.h"

typedef struct {
  uint8_t *data;
  unsigned long len;
  unsigned long int available;
} ArrayU8;

extern ArrayU8 * ArrayU8_new(void);
extern void ArrayU8_extend(ArrayU8 *a, int newlen);
extern void ArrayU8_append(ArrayU8 *a, ArrayU8 *b);
extern void ArrayU8_trim_left(ArrayU8 *a, int amount);
extern void ArrayU8_take_data(ArrayU8 *a, uint8_t **data, int data_len);
extern void ArrayU8_cleanup(ArrayU8 **a);
#define ArrayU8_free(x) ArrayU8_cleanup((x))
extern String * ArrayU8_to_string(ArrayU8 **a);
extern String * ArrayU8_extract_string(ArrayU8 *a, int i1, int i2);
extern int ArrayU8_search(ArrayU8 *a, int offset, const ArrayU8 *target);
extern void ArrayU8_extract_uint32le(ArrayU8 *a, int offset, uint32_t *value);

/* FIXME: Can probably use varargs to figure out n value for .len and .available. */
#define ArrayU8_temp_const(x, n)  & (ArrayU8) { .data = (uint8_t*)x, .len = n, .available = n } 

/* String version. */
#define ArrayU8_temp_string(x)  & (ArrayU8) { .data = (uint8_t*)x, .len = strlen(x), .available = strlen(x) } 

#define ArrayU8_append_bytes(a, ...)  do { uint8_t tmp[] = { __VA_ARGS__ }; ArrayU8_append(a, ArrayU8_temp_const(tmp, sizeof(tmp))); } while (0)

#endif
