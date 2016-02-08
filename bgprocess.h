#ifndef _CTI_BGPROCESS_H_
#define _CTI_BGPROCESS_H_

extern void bgstartv(char * cmdargs[], int * pidptr);
extern void bgstart(const char * cmdline, int * pidptr);

extern void bgstopsigtimeout(int * pidptr, int signal, int timeout_seconds);
extern void bgstop(int * pidptr); /* Calls above with signal=SIGTERM, timeout_seconds=3 */

extern void bgstart_pidfile(const char *cmdline, const char *pidfile);
extern void bgstop_pidfile(const char *pidfile);

extern void bgreap(void);

#endif
