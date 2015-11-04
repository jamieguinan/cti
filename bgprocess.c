/* Simple API for background processes. */
#include <unistd.h>		/* fork */
#include <string.h>		/* strlen, strcpy */
#include <stdio.h>		/* fprintf, NULL */
#include <sys/types.h>		/* kill, wait */
#include <signal.h>		/* kill */
#include <sys/wait.h>		/* wait */

#include "bgprocess.h"

void bgprocess(const char * cmdline, int * pidptr)
{
  char *args[64];
  int cclen = strlen(cmdline)+1;
  //printf("cmdcopy is %d bytes\n", cclen);
  char cmdcopy[cclen];
  strcpy(cmdcopy, cmdline);
  int i;
  char *p = cmdcopy;

  for (i=0; i < 63; i++) {
    args[i] = p;
    p = strchr(p, ' ');
    if (p) {
      *p = 0;
      p++;
      if (*p == 0) {
	break;
      }
    }
    else {
      break;
    }
    //printf("%d: %s\n", i, args[i]);
  }
  //printf("%d: %s\n", i, args[i]);

  if (i==63) {
    fprintf(stderr, "%s: maximum 64 cmdline components\n", __func__);
    return;
  }

  args[i+1] = NULL;

  pid_t newpid = fork();
  switch(newpid) {
  case -1:
    perror("fork");
    break;
  case 0:
    /* child */
    execvp(args[0], args);
    break;
  default:
    /* parent */
    *pidptr = newpid;
    break;
  }
}


void bgstop(int * pidptr)
{
  int pid = *pidptr;
  int status = 0;
  int i;
  int tries = 3;
  kill(pid, SIGTERM);
  for (i=0; i<tries; i++) {
    if (waitpid(pid, &status, WNOHANG) == pid) { break; }
    sleep(1);
  }

  if (i == tries) {
    fprintf(stderr, "pid %d did not exit after %d seconds, sending SIGKILL\n", pid, tries);
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
  }

  *pidptr = -1;
}
