/*
 * Test module for ipc.c.
 * gcc -O0 -ggdb -Wall ipc_test.c -o ipc_test -lpthread
 */
#include <stdio.h>		/* printf */
#include <sys/types.h>          /* socketpair */
#include <sys/socket.h>		/* socketpair */
#include <pthread.h>		/* pthread_create */
#include <time.h>		/* nanosleep */
#include <signal.h>		/* signal */

#include "ipc.c"

#define table_size(x) (sizeof(x)/sizeof(x[0]))

int thread_done = 0;

void sigpipe_handler(int sig) 
{
  printf("sigpipe ignored\n");
}

static void * recv_test(void *vp)
{
  int * sv = vp;
  int fd = sv[1];
  uint8_t * message = NULL;
  uint32_t message_length = 0;
  int rc = ipc_recv(fd, &message, &message_length, 1000);
  printf("%s: rc=%d\n", __func__, rc);
  close(fd);
  thread_done = 1;
  return 0L;
}

int main()
{
  signal(SIGPIPE, sigpipe_handler);

  /* Send 0 byte message. */
  /* For x in { 1, 2, ... }
     synthesize send,
       full message
       half message then close socket
       half message then stall
     send full message using API
  */
  
  int send_sizes[] = { 1, 2, 3, 4, 5, 6, 8, 
		       32, 64, 249, 250, 251, 253, 255, 256, 257, 1024, 16000, 
		       31995, 31996, 31997, 31998,  31999, 32000, 32001, 
		       100000, 1000000 };


  int i;
  int mode;

  char * modes[] = {"synth", "half then close", "half then stall", "api"};

  for (i=0; i < table_size(send_sizes); i++) {
    printf("\n======================\n");
    for (mode=0; mode < 4; mode++) {
      thread_done = 0;
      int j;
      int ss = send_sizes[i];
      int code_size = 1;
      int len_size = 0;
      pthread_t thread;

      printf("\nmode %d (%s), send size: %d\n", mode, modes[mode], ss);
      if (ss > 250) {
	len_size = 4;
      }
      uint8_t * synth_buffer = malloc(code_size+len_size+ss);
      for (j=0; j < ss; j++) {
	synth_buffer[j+code_size+len_size] = (j % 256);
      }
    
      if (ss <= 250) {
	synth_buffer[0] = ss;
      }
      else {
	synth_buffer[0] = 251;
	synth_buffer[1] = ss & 0xff;
	synth_buffer[2] = (ss >> 8) & 0xff;
	synth_buffer[3] = (ss >> 16) & 0xff;
	synth_buffer[4] = (ss >> 24) & 0xff;
      }

      int sv[2];
      int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
      if (rc == -1) {
	perror("socketpair"); break;
      }
      int fd = sv[0];
      pthread_create(&thread, NULL, recv_test, (void*)sv);
      pthread_detach(thread);

      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);

      if (mode == 0) {
	write(fd, synth_buffer, code_size+len_size+ss);
      }
      else if (mode == 1) {
	write(fd, synth_buffer, (code_size+len_size+ss)/2);
	close(fd);
      }
      else if (mode == 2) {
	write(fd, synth_buffer, (code_size+len_size+ss)/2);
	sleep(2);
      }
      else if (mode == 3) {
	{ 
	  int i;
	  int chksum = 0;
	  for (i=0; i < ss; i++) {
	    chksum = chksum + synth_buffer[i+code_size+len_size];
	  }
	  printf("send checksum=%d\n", chksum);
	}

	ipc_send(fd, synth_buffer+code_size+len_size, ss, 1000);
      }

      while (!thread_done) {}
      close(fd);
    }   
  }

  printf("%d/%ld\n", i, table_size(send_sizes));

  return 0;
}
