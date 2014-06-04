#ifndef _SIGNALS_H_
#define _SIGNALS_H_

extern void Signals_init(void);

extern void Signals_handle(int signo, void (*handler)(int));

#endif
