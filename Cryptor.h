#ifndef _CRYPTOR_H
#define _CRYPTOR_H

#include "Array.h"

typedef struct {
  ArrayU8 *cipher;
} Cryptor_context;

/* API for direct use. */
void Cryptor_context_init(Cryptor_context *ctx, String *filename);
void Cryptor_process_data(Cryptor_context *ctx, unsigned int *cipher_index, uint8_t *data, int len);

/* Template initalization. */
extern void Cryptor_init(void);

#endif

