/*
 * Run a subprogram recording stdin/stdout/stderr to files.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define READ 0
#define WRITE 1

int main(int argc, char * argv[], char * envp[])
{
  int i;
  
  /* "cat" is the default subprogram. */
  char * cmd = "/bin/cat";

  /* If a program of the same name with ".ori" suffix exists, run that as the subprogram. */
  char ori[strlen(argv[0]+16)];
  sprintf(ori, "%s.ori", argv[0]);
  if (access(ori, X_OK) == 0) {
    cmd = ori;
  }

  /* Allow override via environment variable */  
  char * ecmd = getenv("GASKET_SUBPROGRAM");
  if (ecmd) {
    cmd = ecmd;
  }

  int pipe0[2];
  int pipe1[2];
  int pipe2[2];
  
  if (pipe(pipe0) != 0) { perror("pipe"); return 1; }
  if (pipe(pipe1) != 0) { perror("pipe"); return 1; }
  if (pipe(pipe2) != 0) { perror("pipe"); return 1; }

  int pid = fork();
  if (pid > 0) {
    FILE * log = fopen("/tmp/gasket-log.txt", "w");
    if (!log) { exit(1); }
    int logs[3];
    for (i=0; i < 3; i++) {
      char logname[128];
      sprintf(logname, "/tmp/gasket-%d.bin", i);
      logs[i] = open(logname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
      if (logs[i] == -1) {
	fprintf(log, "open(%s) failed\n", logname);
	exit(1);
      }
    }

    /* Parent */
    close(pipe0[READ]);
    close(pipe1[WRITE]);
    close(pipe2[WRITE]);
    char buffer[1024];
    ssize_t rn, wn;
    int is_open[3] = { 1, 1, 1};
    while (1) {
      int nfds = 0;
      struct pollfd pfds[3] = {};
      int ofds[3] = {};
      int which_fd[3] = {};
      /* I don't usually like code lines this wide, but in this case
	 it helps to make sure everything is correct by visual
	 inspection. */
      if (is_open[0]) { pfds[nfds].fd = 0;           ofds[nfds] = pipe0[WRITE]; pfds[nfds].events = POLLIN|POLLERR; which_fd[nfds] = 0; nfds++; }
      if (is_open[1]) { pfds[nfds].fd = pipe1[READ]; ofds[nfds] = 1;            pfds[nfds].events = POLLIN|POLLERR; which_fd[nfds] = 1; nfds++; }
      if (is_open[2]) { pfds[nfds].fd = pipe2[READ]; ofds[nfds] = 2;            pfds[nfds].events = POLLIN|POLLERR; which_fd[nfds] = 2; nfds++; }
      
      if (nfds == 0) {
	fprintf(log, "[Done]\n");
	close(2);
	break;
      }

      fprintf(log, "[is_open = { %d, %d, %d}]\n", is_open[0], is_open[1], is_open[2]);
      for (i=0; i < nfds; i++) {
	fprintf(log, "[pfds[%d]: .fd=%d]\n", i, pfds[i].fd);
      }
      
      int n = poll(pfds, nfds, 10000);
      if (n == 0) {
	continue;
      }
      else if (n==-1){
	perror("poll");
	continue;
      }
      else {
	fprintf(log, "[n=%d]\n", n);
      }

      for (i=0; i < nfds; i++) {
	if (pfds[i].revents & POLLIN) {
	  /* Parent/shell stdin */
	  rn = read(pfds[i].fd, buffer, sizeof(buffer));
	  fprintf(log, "[%zd bytes on %d]\n", rn, pfds[i].fd);
	  if (rn == 0) {
	    /* EOF. */
	    fprintf(log, "[closing %d and %d]\n", pfds[i].fd, ofds[i]);
	    close(pfds[i].fd);
	    close(ofds[i]);
	    is_open[which_fd[i]] = 0;
	  }
	  else {
	    wn = write(ofds[i], buffer, rn);
	    if (wn != rn) {
	      fprintf(log, "[write error]\n");
	    }
	    wn = write(logs[which_fd[i]], buffer, rn);
	  }
	}
	else if (pfds[i].revents & POLLERR) {
	  fprintf(log, "[err on %d]\n", pfds[i].fd);
	}
	else if (pfds[i].revents & POLLHUP) {
	  fprintf(log, "[hup on %d]\n", pfds[i].fd);
	  close(pfds[i].fd);
	  if (ofds[i] != 2) {
	    close(ofds[i]);
	  }
	  is_open[which_fd[i]] = 0;
	}
      }
    }

    close(logs[2]);
    close(logs[1]);
    close(logs[0]);
    fclose(log);
  }
  else if (pid == 0) {
    /* Child */
    close(pipe0[WRITE]); dup2(pipe0[READ], 0);  close(pipe0[READ]);
    close(pipe1[READ]);  dup2(pipe1[WRITE], 1); close(pipe1[WRITE]);
    close(pipe2[READ]);  dup2(pipe2[WRITE], 2); close(pipe2[WRITE]);
    argv[0] = cmd;
    execvpe(cmd, argv, envp);
    fprintf(stderr, "execvpe error (%s)\n", argv[0]);
  }
  else {
    perror("fork");
  }

  return 0;
}
