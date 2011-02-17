#include "Signals.h"

#include <stdio.h>
#include <stdlib.h>		/* malloc */
#include <signal.h>
#include <unistd.h>

// #define __USE_GNU

//#include <ucontext.h>

#include <execinfo.h>

extern int asize1;

extern const char *argv0;

static const char *signames[] = {
  [6] = "abort",
  [11] = "segfault",
};

void handler1(int signum, siginfo_t *info, void *context)
{
  ucontext_t *uc = (ucontext_t *)context;
  const char *signame = signames[signum];
  if (!signame) {
    signame = "unknown";
  }
  fprintf(stderr, "%s: user context available at 0x%lx\n", 
	  signame, (long)uc);

  fprintf(stderr, "asize1=%d\n", asize1);

#ifdef __pentium4__
#warning Pentium4
  void **buffer = malloc(256*sizeof(void*));
  int n = backtrace(buffer, 256);
  if (n > 0) {
    int j;
    char **btsyms = backtrace_symbols(buffer, n);
    for (j=0; j < n; j++) {
      puts(btsyms[j]);
    }
    // pause();
  }
#endif

  //while (1) {
    // fprintf(stderr, "RSP=0x%lx\n", uc->uc_mcontext.gregs[REG_RSP]);  // amd64
    // uc->uc_link ...
  //break;
  //}

  // Give instructions for attaching with GDB, then suspend.
  if (0) {
    int pid = getpid();
    printf("To resume this program with debugger:\n  kill -SIGCONT %d\n  gdb -nx %s\n  (gdb) attach %d\n",
	   pid, argv0, pid);
    kill(pid, SIGSTOP);
    while (1) {
      sleep(1);
    }
  }

  _exit(1);
}

static struct sigaction oldact_SIGSEGV;
static struct sigaction act_SIGSEGV = {
  .sa_sigaction = handler1,
  // .sa_mask = { 0 } ,
  .sa_flags = SA_SIGINFO,
};

static struct sigaction oldact_SIGABRT;
static struct sigaction act_SIGABRT = {
  .sa_sigaction = handler1,
  // .sa_mask = { 0 } ,
  .sa_flags = SA_SIGINFO,
};

void Signals_init(void)
{
  int rc;
  rc = sigaction(SIGSEGV, &act_SIGSEGV, &oldact_SIGSEGV);
  rc = sigaction(SIGABRT, &act_SIGABRT, &oldact_SIGABRT);
}
