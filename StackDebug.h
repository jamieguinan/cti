#ifndef _STACKDEBUG_H_
#define _STACKDEBUG_H_

#ifdef USE_STACK_DEBUG
extern void StackDebug(void);
#else
#define StackDebug(x)
#endif

#endif
