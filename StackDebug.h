#ifndef _STACKDEBUG_H_
#define _STACKDEBUG_H_

#ifdef USE_STACK_DEBUG
extern void StackDebug(void);
extern void StackDebug2(void * ptr, const char * text);
#else
#define StackDebug(x)
#define StackDebug2(x, y)
#endif

#endif
