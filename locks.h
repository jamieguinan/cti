#ifndef _CTI_LOCK_H_
#define _CTI_LOCK_H_

#ifdef __linux__
#include <pthread.h>
#include <semaphore.h>
typedef struct {
  pthread_mutex_t mlock;
} Lock;

typedef struct {
  pthread_cond_t event;
} Event;

typedef struct {
  sem_t wlock;
} WaitLock;
#endif

#ifdef __APPLE__
#include <pthread.h>
typedef struct {
  pthread_mutex_t lock;
} Lock;

typedef struct {
  pthread_cond_t lock;
  pthread_mutex_t lockm;
} WaitLock;
#endif

#ifdef __MINGW32__
typedef struct {
} Lock;

typedef struct {
} Event;

typedef struct {
} WaitLock;
#endif

#ifdef __MSP430__
/* Just for testing the compiler... */
typedef struct {
} Lock;

typedef struct {
} Event;

typedef struct {
} WaitLock;
#endif

extern void Lock_init(Lock *lock);
extern void Lock_acquire(Lock *lock);
extern void Lock_release(Lock *lock);

extern void Lock_release__event_wait__lock_acquire(Lock *lock, Event *ev);
extern void Event_signal(Event *ev);

extern void WaitLock_init(WaitLock *w);
extern void WaitLock_wait(WaitLock *w);
extern void WaitLock_wake(WaitLock *w);

#endif
