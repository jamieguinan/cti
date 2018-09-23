//#include "CTI.h"
#include "ArrayU8.h"
#include "Mem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

ArrayU8 * ArrayU8_new(void)
{
  ArrayU8 *a = Mem_calloc(1, sizeof(*a));
  return a;
}


void ArrayU8_extend(ArrayU8 *a, int newlen)
{
  if (a->available == 0) {
    a->available = 32;          /* Reasonable default. */
  }

  while (newlen > a->available) {
    a->available *= 2;
  }

  if (a->data) {
    a->data = Mem_realloc(a->data, a->available);
  }
  else {
    a->data = Mem_malloc(a->available);
  }

  a->len = newlen;
}


void ArrayU8_append(ArrayU8 *a, ArrayU8 *b)
{
  int tmplen = a->len;
  ArrayU8_extend(a, a->len + b->len);
  memcpy(a->data + tmplen, b->data, b->len);
}

void ArrayU8_trim_left(ArrayU8 *a, int amount)
{
  if (amount > a->len) {
    fprintf(stderr, "%s: tried to trim %d bytes but array only has %ld bytes!\n", __func__, amount, a->len);
  }
  memmove(a->data, a->data+amount, a->len-amount);
  a->len -= amount;
  memset(a->data+a->len, 0, amount);
  /* a->available stays the same! */
}

void ArrayU8_take_data(ArrayU8 *a, uint8_t **data, int data_len)
{
  if (a->data) {
    Mem_free(a->data);
  }
  a->data = *data;
  a->len = a->available = data_len;
  *data = 0L;                   /* Clear caller copy, data belongs to array now. */
}


void ArrayU8_cleanup(ArrayU8 **a)
{
  if ((*a)->data) {
    Mem_free((*a)->data);
  }
  Mem_free(*a);
  *a = 0L;
}


String * ArrayU8_to_string(ArrayU8 **a)
{
  String *s = String_new((char*)(*a)->data);
  ArrayU8_cleanup(a);
  return s;
}

String * ArrayU8_extract_string(ArrayU8 *a, int i1, int i2)
{
  uint8_t tmp = a->data[i2];
  a->data[i2] = '\0';
  String *s = String_new((char*)(a)->data+i1);
  a->data[i2] = tmp;
  return s;
}

int ArrayU8_search(ArrayU8 *a, int offset, const ArrayU8 *target)
{
  int i, j;
  for (i = offset; i <= (a->len - target->len); i++) {
    for (j = 0; j < target->len; j++) {
      if (a->data[i+j] != target->data[j]) {
        break;
      }
    }
    if (j == target->len) {
      return i;
    }
  }
  return -1;
}


void ArrayU8_extract_uint8(ArrayU8 *a, int offset, uint8_t *value)
{
  if (a->len < (offset)) {
    fprintf(stderr, "*** %s bounds error\n", __func__);
    return;
  }
  *value = a->data[offset];
}

void ArrayU8_extract_uint32le(ArrayU8 *a, int offset, uint32_t *value)
{
  if (a->len < (offset+4)) {
    fprintf(stderr, "*** %s bounds error\n", __func__);
    return;
  }
  *value = (uint32_t) ( (a->data[offset+3] << 24) |
                        (a->data[offset+2] << 16) |
                        (a->data[offset+1] << 8) |
                        (a->data[offset+0] << 0) );
}
