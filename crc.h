#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>

uint32_t init_crc32();
uint32_t update_crc32(uint32_t crc, const unsigned char *buf,
		      int len);
uint32_t value_crc32(uint32_t crc);

#endif
