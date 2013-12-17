#include "locks.h"
#include <stdio.h>
#include <stdlib.h>


void Lock_init(Lock *lock)
{
#ifdef __linux__
  pthread_mutex_init(&lock->mlock, NULL);
#endif
}


void Lock_acquire(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_lock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_lock returned %d\n", rc);
    exit(1);
  }
#endif
}

void Lock_release(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_unlock(&lock->mlock);
  if (rc != 0) {
    perror("pthread_mutex_unlock");
    exit(1);
  }
#endif
}

void Lock_release__event_wait__lock_acquire(Lock *lock, Event *event)
{
#ifdef __linux__
  int rc;
  rc = pthread_cond_wait(&event->event, &lock->mlock);
  if (rc != 0) {
    perror("pthread_cond_wait");
    exit(1);
  }
#endif
}

void Event_signal(Event *ev)
{
#ifdef __linux__
  pthread_cond_broadcast(&ev->event);
#endif
}

void WaitLock_init(WaitLock *w)
{
#ifdef __linux__
#endif
}

void WaitLock_wait(WaitLock *w)
{
#ifdef __linux__
  int rc;
  rc = sem_wait(&w->wlock);
  if (rc != 0) {
    perror("sem_wait");
    exit(1);
  }
#endif
}

void WaitLock_wake(WaitLock *w)
{
#ifdef __APPLE__
  if (pthread_cond_signal(&input->parent->notification_cond) != 0) {
    perror("pthread_cond_signal");
  }
#endif
#ifdef __linux__
  int rc;
  /* Wake receiver. */
  rc = sem_post(&w->wlock);
  if (rc != 0) {
    perror("sem_wait");
    exit(1);
  }
#endif
}
