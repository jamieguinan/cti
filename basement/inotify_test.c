/* Non-comprehensive inotify test. */
#include <sys/inotify.h>
#include <unistd.h>
#include <stdio.h>

void dump_event_mask(uint32_t mask)
{
  if (mask & IN_MODIFY) {  printf("0x%08x IN_MODIFY\n", mask & IN_MODIFY); }
  if (mask & IN_DELETE) {  printf("0x%08x IN_DELETE\n", mask & IN_DELETE); }
  if (mask & IN_MOVE) {  printf("0x%08x IN_MOVE\n", mask & IN_MOVE); }
  if (mask & IN_DELETE_SELF) {  printf("0x%08x IN_DELETE_SELF\n", mask & IN_DELETE_SELF); }
}

int main(int argc, char * argv[])
{
  int i;
  const char * testpath = "testme.txt";
  struct inotify_event iev = {};
  int ifd = inotify_init();

  FILE * f = fopen(testpath, "w");
  if (f) {
    fprintf(f, "This is for inotify testing.\n");
    fclose(f);
  }
  else {
    perror("could not create test file");
    return 1;
  }

  printf("watching %s\n", testpath);
  inotify_add_watch(ifd, testpath, IN_MODIFY|IN_DELETE|IN_DELETE_SELF|IN_MOVE);  

  for (i=1; i < argc; i++) {
    if (access(argv[i], R_OK) == 0) {
      printf("watching %s\n", argv[i]);
      inotify_add_watch(ifd, argv[i], IN_MODIFY|IN_DELETE|IN_DELETE_SELF|IN_MOVE);
    }
  }

  printf("This should be running in the background, do ctrl-z then \"bg\" if not.\n");
  printf("Try these commands from the shell:\n"
	 "  date >> %s\n"
	 "  rm %s\n",
	 testpath, testpath);

  while (1) {
    char tmp[256] = {};
    if (read(ifd, &iev, sizeof(iev)) >= sizeof(iev)) {
      printf("event mask:\n");
      dump_event_mask(iev.mask);
      printf("len: %d\n", iev.len);
      tmp[0] = iev.name[0];
      if (iev.len > 1) {
	int n = read(ifd, tmp+1, iev.len);
	printf("read %d additional bytes\n", n);
      }
      printf("file: %s\n", tmp);
    }
  }
  
  return 0;
}
