#ifndef _STACKDEBUG_H_
#define _STACKDEBUG_H_

#ifdef USE_STACK_DEBUG
extern void StackDebug(void);
extern void StackDebug2(void * ptr, const char * text);
extern void StackDebug3up(void *ptr, const char *type, ssize_t size);
extern void StackDebug3down(void *ptr);
#else
#define StackDebug(x)
#define StackDebug2(x, y)
#define StackDebug3up(p, t, s)
#define StackDebug3down(p)
#endif

#endif
