#include <stdio.h>
#include <string.h>

void argsplit1(const char * cmdline)
{
  char *args[64];
  int cclen = strlen(cmdline)+1;
  //printf("cmdcopy is %d bytes\n", cclen);
  char cmdcopy[cclen];
  strcpy(cmdcopy, cmdline);
  int i, j;
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

  for (j=0; j < i; j++) {
    printf("argvs[%d]: %s\n", j, args[j]);
  }
}

#define SINGLE_QUOTE '\''
void argsplit2(const char * cmdline)
{
  char *args[64];
  int cclen = strlen(cmdline)+1;
  //printf("cmdcopy is %d bytes\n", cclen);
  char cmdcopy[cclen];
  strcpy(cmdcopy, cmdline);
  int in_quote = 0;
  int in_arg = 0;
  char *p = cmdcopy;
  int j;
  int i = 0;
  while (i < 63 && *p) {
    if (*p == ' ') {
      if (!in_quote) {
	*p = 0;
	in_arg = 0;
      }
    }
    else if (*p == SINGLE_QUOTE) {
      if (in_quote) {
	*p = 0;
	in_quote = 0;
      }
      else {
	in_quote = 1;
      }
    }
    else {
      if (!in_arg) {
	args[i++] = p;
	in_arg = 1;
      }
    }
    p++;
  }
  //printf("%d: %s\n", i, args[i]);

  if (i==63) {
    fprintf(stderr, "%s: maximum 64 cmdline components\n", __func__);
    return;
  }

  args[i+1] = NULL;

  for (j=0; j < i; j++) {
    printf("argvs[%d]: %s\n", j, args[j]);
  }
}

int main()
{
  char * cmd = "empty -p empty_omxplayer.pid"
    " -i /tmp/pb.in -o /tmp/pb.out"
    " -f livestreamer -np '/usr/bin/omxplayer --layer 1 --lavfdopts probesize:250000'"
    " 'https://youtube.com/watch?v=yaddayaddayadda'"
    " best";
  argsplit1(cmd);
  argsplit2(cmd);
  return 0;
}
