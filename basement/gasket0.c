/*
 * This is a simple gasket program that only copies stdin to the sub
 * process.  "gasket.c" is a longer example with all 3 std streams,
 * and also logs them to files.
 */
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char * argv[], char * envp[])
{
  int pipe0[2];
  int pipe1[2];
  int pipe2[2];
  
  char * cmd = "rev";
  char * ecmd = getenv("GASKET_PROGRAM");
  if (ecmd) {
    cmd = ecmd;
  }

  pipe(pipe0);

  int pid = fork();
  if (pid > 0) {
    /* Parent */
    close(pipe0[0]);
    char buf;
    while (read(0, &buf, 1) > 0) {
      write(pipe0[1], &buf, 1);
    }
  }
  else if (pid == 0) {
    /* Child */
    close(pipe0[1]);
    dup2(pipe0[0], 0);
    argv[0] = cmd;
    execvpe(cmd, argv, envp);
    perror("execvpe");
  }
  else {
    perror("fork");
  }

  return 0;
}
