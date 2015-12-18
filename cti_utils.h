#ifndef _CTI_UTILS_H_
#define _CTI_UTILS_H_

extern void cti_msleep(unsigned long msecs);
extern void cti_usleep(unsigned long usecs);

#define cti_table_size(x) (sizeof(x)/sizeof(x[0]))
#define cti_min(a, b)  ( (a) < (b) ? (a) : (b) )
#define cti_max(a, b)  ( (a) > (b) ? (a) : (b) )

#endif
