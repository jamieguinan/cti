#ifndef _CTI_BGPROCESS_H_
#define _CTI_BGPROCESS_H_

extern void bgprocessv(char * cmdargs[], int * pidptr);
extern void bgprocess(const char * cmdline, int * pidptr);

extern void bgstopsigtimeout(int * pidptr, int signal, int timeout_seconds);
extern void bgstop(int * pidptr); /* Calls above with signal=SIGTERM, timeout_seconds=3 */

#endif
