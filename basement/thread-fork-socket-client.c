/* Client test for socket/threads issue. */
#include <stdio.h>		/* fprintf */
#include <pthread.h>		/* threads */
#include <unistd.h>		/* sleep */
#include <stdlib.h>		/* system */
#include <signal.h>
#include <poll.h>		/* poll */

#include "SourceSink.h"
#include "cti_ipc.h"
#include "cti_utils.h"

static char * playback_port = "localhost:5101";

static pthread_t t1, t2;

static void * connect_loop(void *vp)
{
  Comm * c = NULL;
  unsigned char xreq = 0;
  
  while (1) {
    Comm_free(&c);
    c = Comm_new(playback_port);

    if (!IO_ok(c)){
      /* FIXME: Report error? */
      sleep(1);
      continue;
    }

    xreq += 1;
    printf("xreq %d\n", xreq);
    
    if (write(c->io.s, &xreq, 1) != 1) {
      perror("write");
      sleep(1);
      continue;
    }

    struct pollfd fds[1];
    fds[0].fd = c->io.s;
    fds[0].events = POLLIN|POLLERR;
    if (poll(fds, 1, 1.3 * 1000) <= 0) {
      printf("poll timeout\n");
    }
  }

  return NULL;
}


static void * sleep_loop(void *vp)
{
  while (1) {
    int rc;
    rc = system("sleep 5");
    printf("sleep rc %d\n", rc);
    sleep(1);			/* Need this to to allow interrupting with ctrl-c */
  }

  return NULL;
}

void sigchld_handler(int signo)
{
  printf("got SIGCHLD\n");
}

int main(int argc, char * argv[])
{
  char * base = getenv("HOME");
  if (!base) { base = ""; }

  signal(SIGCHLD, SIG_IGN);

  pthread_create(&t1, NULL, connect_loop, NULL);
  pthread_detach(t1);

  pthread_create(&t2, NULL, sleep_loop, NULL);
  pthread_detach(t2);
  
  while (1) {
    sleep(1);
  }
  return 0;
}
