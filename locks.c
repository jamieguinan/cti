#include "locks.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>		/* kill */
#if defined(__APPLE__)
#include <unistd.h>		/* getpid */
#endif

#ifdef LOCKS_STACKDEBUG
#include "StackDebug.h"
#else
#define Stack_dump() do {} while (0)
#endif

void Lock_init(Lock *lock)
{
#if defined(__linux__) || defined(__APPLE__)
  int rc;
  rc = pthread_mutex_init(&lock->mlock, NULL);
  if (rc != 0) {
    perror("pthread_mutex_init");
    kill(0, 3);
  }
#else
#error Lock_init not defined for this platform.
#endif
}

void Lock_destroy(Lock *lock)
{
#if defined(__linux__) || defined(__APPLE__)
  pthread_mutex_destroy(&lock->mlock);
#else
#error Lock_destroy is not defined for this platform.
#endif
}

void Lock_acquire(Lock *lock)
{
#if defined(__linux__) || defined(__APPLE__)
  int rc;
  rc = pthread_mutex_lock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_lock returned %d\n", rc);
    Stack_dump();
    exit(1);
  }
#else
#error Lock_acquire is not defined for this platform.
#endif
}

void Lock_release(Lock *lock)
{
#if defined(__linux__) || defined(__APPLE__)
  int rc;
  rc = pthread_mutex_unlock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_unlock returned %d\n", rc);
    Stack_dump();
    exit(1);
  }
#else
#error Lock_release is not defined for this platform.
#endif
}

void Lock_release__event_wait__lock_acquire(Lock *lock, Event *event)
{
#if defined(__linux__) || defined(__APPLE__)
  int rc;
  printf("pthread_cond_wait %p %p\n", &event->event, &lock->mlock);
  rc = pthread_cond_wait(&event->event, &lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_cond_wait returned %d\n", rc);
    Stack_dump();
    abort();
    exit(1);
  }
#else
#error Lock_release__event_wait__lock_acquire is not defined for this platform.
#endif
}


void Event_init(Event *ev)
{
#if defined(__linux__) || defined(__APPLE__)
  pthread_cond_init(&ev->event, NULL);
#else
#error Event_init is not defined for this platform.
#endif
}


void Event_destroy(Event *ev)
{
#if defined(__linux__) || defined(__APPLE__)
  pthread_cond_destroy(&ev->event);
#else
#error Event_destory is not defined for this platform.
#endif
}


void Event_signal(Event *ev)
{
#if defined(__linux__) || defined(__APPLE__)
  pthread_cond_broadcast(&ev->event);
#else
#error Event_signal is not defined for this platform.
#endif
}

/* Semaphores */
void Sem_init(Sem *sem)
{
#if defined(__linux__)
  sem->sem = &sem->_sem;
  int rc = sem_init(sem->sem,
                    0, /* shared between threads only */
                    0  /* initial value */
    );
  if (rc != 0) {
    perror("sem_init");
    exit(1);
  }
#elif defined(__APPLE__)
  /* macOS doesn't support anonymous semaphores with sem_init(), only
     sem_open() which requires a system-wide unique name. My solution
     is to generate a unique name using pid and address of the
     structure. */
  char name[128];
  sprintf(name, "%ld%p", (long)getpid(), sem);
  sem->sem = sem_open(name, O_CREAT|O_EXCL, 0700, 0);
  if (SEM_FAILED == sem->sem) {
    perror("sem_open");
    exit(1);
  }
#else
#error Sem_init is not defined for this platform.
#endif
}


void Sem_post(Sem *sem)
{
#if defined(__linux__) || defined(__APPLE__)
  if (sem_post(sem->sem) != 0) { perror("sem_post"); }
#else
#error Sem_post is not defined for this platform.
#endif
}


void Sem_wait(Sem *sem)
{
#if defined(__linux__) || defined(__APPLE__)
  if (sem_wait(sem->sem) != 0) { perror("sem_wait"); }
#else
#error Sem_wait is not defined for this platform.
#endif
}


void Sem_destroy(Sem *sem)
{
#if defined(__linux__)
  if (sem_destroy(sem->sem) != 0) { perror("sem_destroy"); }
#elif defined(__APPLE__)
  if (sem_close(sem->sem) != 0) { perror("sem_close"); }
#else
#error Sem_destroy is not defined for this platform.
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
