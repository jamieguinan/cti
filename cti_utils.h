#ifndef _CTI_UTILS_H_
#define _CTI_UTILS_H_

extern void cti_msleep(unsigned long msecs);
extern void cti_usleep(unsigned long usecs);
extern void cti_getdoubletime(double * tdest);
extern unsigned char cti_extract_decimal_uchar(const char * input, int offset, int count, int *err);
extern unsigned char cti_extract_hex_uchar(const char * input, int offset, int count, int *err);
extern void cti_urandom_fill(void * ptr, int size, int * err);

#define cti_table_size(x) (sizeof(x)/sizeof(x[0]))
#define cti_min(a, b)  ( (a) < (b) ? (a) : (b) )
#define cti_max(a, b)  ( (a) > (b) ? (a) : (b) )

#endif
