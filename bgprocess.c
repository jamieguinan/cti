/* Simple API for background processes. */
#include <unistd.h>		/* fork */
#include <string.h>		/* strlen, strcpy */
#include <stdio.h>		/* fprintf, NULL */
#include <sys/types.h>		/* kill, wait */
#include <signal.h>		/* kill */
#include <sys/wait.h>		/* wait */

#include "bgprocess.h"

void bgstartv(char * args[], int * pidptr)
{
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

void bgstart(const char * cmdline, int * pidptr)
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

  bgstartv(args, pidptr);
}


void bgstopsigtimeout(int * pidptr, int signal, int timeout_seconds)
{
  int pid = *pidptr;
  int status = 0;
  int i;
  int rc=-1;

  kill(pid, signal);
  for (i=0; i<timeout_seconds; i++) {
    rc = waitpid(pid, &status, WNOHANG);
    if (rc == -1) {
      perror("waitpid");
    }
    else if (rc == pid) {
      break; 
    }
    sleep(1);
  }

  if (i == timeout_seconds && rc != -1) {
    fprintf(stderr, "pid %d did not exit after %d seconds, sending SIGKILL\n", pid, timeout_seconds);
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
  }

  *pidptr = -1;
}


void bgstop(int * pidptr)
{
  bgstopsigtimeout(pidptr, SIGTERM, 3);
}


void bgstart_pidfile(const char * cmd, const char *pidfile)
{
  int pid;
  FILE * f = fopen(pidfile, "w");
  if (!f) {
    fprintf(stderr, "%s: could not open pidfile %s\n", __func__, pidfile);
    return;
  }
  bgstart(cmd, &pid);
  fprintf(f, "%d\n", pid);
  fclose(f);
}


void bgstop_pidfile(const char *pidfile)
{
  char buffer[256] = {};
  int pid;
  FILE * f = fopen(pidfile, "r");
  if (!f) {
    fprintf(stderr, "%s: could not open pidfile %s\n", __func__, pidfile);
    return;
  }
  fgets(buffer, sizeof(buffer), f);
  if (sscanf(buffer, "%d", &pid) == 1) {
    bgstop(&pid);
  }
  unlink(pidfile);
}
