#include "Signals.h"
#include "StackDebug.h"

#include <stdio.h>
#include <stdlib.h>		/* malloc */
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif

extern const char *argv0;

static const char *signames[64] = {
  // [SIGHUP] = "SIGHUP",
  [SIGINT] = "SIGINT",
  // [SIGQUIT] = "SIGQUIT",
  [SIGABRT] = "SIGABRT",
  [SIGSEGV] = "SIGSEGV",
};


static void shutdown_handler(int signo)
{
  const char *signame = signames[signo];
  if (!signame) {
    signame = "unknown";
  }

  const char threadname[17] = "thread";
#ifdef HAVE_PRCTL
  {
    prctl(PR_GET_NAME, threadname);
  }
#endif

  fprintf(stderr, "%s: caught signal %d (%s)\n", threadname, signo, signame);
  _exit(signo);
}

static void usr1_handler(int signo)
{
#ifdef USE_STACKDEBUG
  StackDebug();
#endif
}


void Signals_handle(int signo, void (*handler)(int))
{

  if (signal(signo, handler) == SIG_ERR) {
    fprintf(stderr, "signal(%d) failed\n", signo); 
  }
}

void Signals_init(void)
{
  Signals_handle(SIGSEGV, shutdown_handler);
  Signals_handle(SIGABRT, shutdown_handler);
  Signals_handle(SIGUSR1, usr1_handler);
}
