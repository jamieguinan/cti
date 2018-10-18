#include "locks.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef LOCKS_STACKDEBUG
#include "StackDebug.h"
#else
#define Stack_dump() do {} while (0)
#endif

void Lock_init(Lock *lock)
{
#ifdef __linux__
  pthread_mutex_init(&lock->mlock, NULL);
#else
#error __func__ is not defined for this platform.
#endif
}

void Lock_destroy(Lock *lock)
{
#ifdef __linux__
  pthread_mutex_destroy(&lock->mlock);
#else
#error __func__ is not defined for this platform.
#endif
}

void Lock_acquire(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_lock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_lock returned %d\n", rc);
    Stack_dump();
    exit(1);
  }
#else
#error __func__ is not defined for this platform.
#endif
}

void Lock_release(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_unlock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_unlock returned %d\n", rc);
    Stack_dump();
 exit(1);
  }
#else
#error __func__ is not defined for this platform.
#endif
}

void Lock_release__event_wait__lock_acquire(Lock *lock, Event *event)
{
#ifdef __linux__
  int rc;
  rc = pthread_cond_wait(&event->event, &lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_cond_wait returned %d\n", rc);
    Stack_dump();
    exit(1);
  }
#else
#error __func__ is not defined for this platform.
#endif
}


void Event_init(Event *ev)
{
#ifdef __linux__
  pthread_cond_init(&ev->event, NULL);
#else
#error __func__ is not defined for this platform.
#endif
}


void Event_destroy(Event *ev)
{
#ifdef __linux__
  pthread_cond_destroy(&ev->event);
#else
#error __func__ is not defined for this platform.
#endif
}


void Event_signal(Event *ev)
{
#ifdef __linux__
  pthread_cond_broadcast(&ev->event);
#else
#error __func__ is not defined for this platform.
#endif
}

/* Semaphores */
void Sem_init(Sem *sem)
{
#ifdef __linux__
  int rc = sem_init(&sem->sem,
                    0, /* shared between threads only */
                    0  /* initial value */
    );
  if (rc != 0) {
    perror("sem_init");
    exit(1);
  }
#else
#error __func__ is not defined for this platform.
#endif
}


void Sem_post(Sem *sem)
{
#ifdef __linux__
  if (sem_post(&sem->sem) != 0) { perror("sem_post"); }
#else
#error __func__ is not defined for this platform.
#endif
}


void Sem_wait(Sem *sem)
{
#ifdef __linux__
  if (sem_wait(&sem->sem) != 0) { perror("sem_wait"); }
#else
#error __func__ is not defined for this platform.
#endif
}


void Sem_destroy(Sem *sem)
{
#ifdef __linux__
  if (sem_destroy(&sem->sem) != 0) { perror("sem_destroy"); }
#else
#error __func__ is not defined for this platform.
#endif
}


/* Reference count with lock. */
void LockedRef_init(LockedRef *ref)
{
  ref->_count = 1;
  Lock_init(&ref->lock);
}


void LockedRef_increment(LockedRef *ref, int *count)
{
  Lock_acquire(&ref->lock);
  ref->_count += 1;
  *count = ref->_count;
  Lock_release(&ref->lock);
}


void LockedRef_decrement(LockedRef *ref, int * count)
{
  Lock_acquire(&ref->lock);
  ref->_count -= 1;
  *count = ref->_count;
  Lock_release(&ref->lock);
}
