/* Simple API for background processes. */
#include <unistd.h>		/* fork */
#include <string.h>		/* strlen, strcpy */
#include <stdio.h>		/* fprintf, NULL */
#include <sys/types.h>		/* kill, wait */
#include <signal.h>		/* kill */
#include <sys/wait.h>		/* wait */
#include <errno.h>		/* errno */

#include "bgprocess.h"
#include "cti_utils.h"

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
  int ischild = 1;
  int rc=-1;
  char procpath[1+4+1+24+1];
  sprintf(procpath, "/proc/%d", pid);

  /* Use 1/10 second sleeps in loop. */
  int msleep_period = 10;
  int msleep_count = timeout_seconds * 100;

  /* First, do a waitpid and see if the process has already changed state. */
  rc = waitpid(pid, &status, WNOHANG);

  /* We can use the same results of waitpid() to detect if the
     process detached via setsid or something. */
  if (rc == -1 && errno == ECHILD) {
    ischild = 0;
  }

  /* Is the process even there? */
  if (access(procpath, R_OK) == -1) { 
    fprintf(stderr, "pid %d is already gone\n", pid);
    goto out; 
  }

  /* Deliver the signal */
  fprintf(stderr, "delivering signal %d to pid %d\n", signal, pid);
  kill(pid, signal);

  for (i=0; i < msleep_count; i++) {
    /* Sleep at the top of the loop to give the process a token amount of
       time to clean up and exit. */
    cti_msleep(msleep_period);

    if (ischild) {
      rc = waitpid(pid, &status, WNOHANG);
      if (rc == pid) {
	if (WIFEXITED(status)) { 
	  fprintf(stderr, "pid %d has exited\n", pid);
	  break;
	}
	else {
	  fprintf(stderr, "pid %d changed state but is still active\n", pid);	  
	}
      }
    }
    else {
      /* There is a race condition here, where if the process table numbering
	 has looped around, another process could have filled in the slot. 
	 But this is the case where we're waiting for non-child processes
	 anyway, so its already a grey area. */
      if (access(procpath, R_OK) == -1) { 
	fprintf(stderr, "%s is gone\n", procpath);
	break; 
      }
    }
  }

  fprintf(stderr, "i=%d\n", i);

  if (i == msleep_count) {
    fprintf(stderr, "pid %d did not exit after %d seconds, sending SIGKILL\n", pid, timeout_seconds);
    kill(pid, SIGKILL);
    if (ischild) {
      cti_msleep(msleep_period);
      waitpid(pid, &status, 0);
    }
  }

 out:
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


void bgreap(void)
{
  int status;
  int rc;
  do {
    rc = waitpid(-1, &status, WNOHANG);
    if (rc > 0) {
      fprintf(stderr, "reaped pid %d\n", rc);
    }
  } while (rc != 0);
}