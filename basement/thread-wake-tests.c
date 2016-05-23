/* 
 * Clean this up for testing eventd, inotify, and pipe for intra-process communication.
 */
#include <stdlib.h>		/* getenv */
#include <unistd.h>		/* chdir */
#include <stdio.h>		/* fprintf */
#include <poll.h>		/* poll */
#include <pthread.h>		/* threads */
#include <sys/inotify.h>
#include <sys/eventfd.h>

#include "String.h"
#include "socket_common.h"
#include "cti_ipc.h"
#include "cti_utils.h"
#include "jsmn.h"
#include "jsmn_extra.h"
#include "locks.h"
#include "bgprocess.h"
#include "Regex.h"
#include "localptr.h"

// #define emap(map, value) map[value % cti_table_size(map)]

#undef PBEVENTFD
#undef PBINOTIFY
#define PBPIPE

static struct {
  Lock lock;
  int playing;
  int paused;
  int recording;
  int sequence;
  String * title;
  int64_t run_time;
  int64_t current_time;
  String * clientid;

#ifdef PBINOTIFY
  String * synchronization_path;
  FILE * synchronization_file;
#endif

#ifdef PBPIPE
  int wakepipe[2];
  int wake_count;		/* number of readers */
#endif

#ifdef PBEVENTFD
  int wakefd;
  int wake_count;		/* number of readers */
#endif

  int azap_pid;
  int mux_pid;
  int player_pid;
  int record_pid;
} playbackState = {
  .playing = 0,
  .recording = 0,
  .paused = 0,
  .sequence = 1,
  .azap_pid = -1,
  .mux_pid = -1,
  .player_pid = -1,
  .record_pid = -1,
#ifdef PBPIPE
  .wake_count = 0,
#endif
};

static void set_client(String * clientstr)
{
  String_clear(&playbackState.clientid);
  playbackState.clientid = String_dup(clientstr);
}

static void wake_config(void)
{
  /* playbackState.lock must be held when this is called. */
  playbackState.sequence = (playbackState.sequence % 10000) + 1;
  printf("playbackState.sequence=%d\n", playbackState.sequence);
#ifdef PBINOTIFY
  lseek(fileno(playbackState.synchronization_file), 0L, SEEK_SET);
  write(fileno(playbackState.synchronization_file), ".", 1);
#endif

#ifdef PBPIPE
  int i;
  printf("wake_count=%d\n", playbackState.wake_count);
  for (i=0; i < playbackState.wake_count; i++) {
    write(playbackState.wakepipe[1], ".", 1);
  }
  playbackState.wake_count = 0;
#endif

#ifdef PBEVENTFD
  printf("wake_count=%d\n", playbackState.wake_count);
  uint64_t wake = playbackState.wake_count;
  if (write(playbackState.wakefd, &wake, sizeof(wake)) != sizeof(wake)) {
    perror("eventfd write");
  }
  playbackState.wake_count = 0;
#endif

}

static void sequence_wait(int ipcfd)
{
  struct pollfd fds[4];
  int nfds = 0;
#ifdef PBINOTIFY
  int ifd, wfd;
  ifd = inotify_init();
  if (ifd == -1) {
    perror("inotify_init");
    return;
  }

  wfd = inotify_add_watch(ifd, s(playbackState.synchronization_path), IN_MODIFY);
  printf("ifd=%d\n", ifd);
  printf("wfd=%d\n", wfd);
#endif
  
  fds[0].fd = ipcfd;
  fds[0].events = POLLIN|POLLERR;
  nfds++;

#ifdef PBINOTIFY
  fds[1].fd = ifd;
  fds[1].events = POLLIN|POLLERR;
  nfds++;
#endif

#ifdef PBPIPE
  fds[1].fd = playbackState.wakepipe[0];
  fds[1].events = POLLIN|POLLERR;
  nfds++;
#endif

#ifdef PBEVENTFD
  fds[1].fd = playbackState.wakefd;
  fds[1].events = POLLIN|POLLERR;
  nfds++;
#endif

  int rc = poll(fds, nfds, -1);
  printf("poll rc=%d\n", rc);

  if (fds[0].revents & POLLIN) {
    printf("ipc closed\n");
  }

#ifdef PBEVENTFD
  if (fds[1].revents & POLLIN) {
    uint64_t wake;
    if (read(playbackState.wakefd, &wake, sizeof(wake)) != sizeof(wake)) {
      perror("eventfd read");
    }
    printf("read 1 byte\n");
  }
#endif

#ifdef PBPIPE
  if (fds[1].revents & POLLIN) {
    unsigned char x;
    read(playbackState.wakepipe[0], &x, 1);
    printf("read 1 byte\n");
  }
#endif

#ifdef PBINOTIFY
  inotify_rm_watch(ifd, wfd);
  close(ifd);
#endif
}

#define STOP_PLAYBACK (1<<0)
#define STOP_MUX      (1<<1)
#define STOP_AZAP     (1<<2)
#define STOP_ALL (STOP_PLAYBACK|STOP_MUX|STOP_AZAP)
static void stop_playback_programs(int mask)
{
  /* Stop existing player. */
  if ((mask & STOP_PLAYBACK) && playbackState.player_pid != -1) {
    printf("stopping player...\n");
    /* Cannot stop by player_pid because `empty` forks before starting the
       subprogram. But the pid is still useful for tracking, and 
       any of these commands will work to stop the player. */
    if (0) { system("empty -s -o /tmp/pb.in q"); }
    if (1) { system("killall omxplayer"); }
    if (0) { bgstop_pidfile("empty_omxplayer.pid"); }
    // dbus...
      playbackState.playing = 0;
  }

  /* Stop existing mux. */
  if ((mask & STOP_MUX) && playbackState.mux_pid != -1) {
    //printf("stopping mux pid %d\n", playbackState.mux_pid);
    bgstop(&playbackState.mux_pid);
  }

  /* Stop existing azap. */
  if ((mask & STOP_AZAP) && playbackState.azap_pid != -1) {
    //printf("stopping azap pid %d\n", playbackState.azap_pid);
    bgstop(&playbackState.azap_pid);
  }
}

void command_play_qam(JsmnContext * jc)
{
  localptr(String, player_cmd) = String_value_none();
  localptr(String, mux_cmd) = String_value_none();
  localptr(String, azap_cmd) = String_value_none();
  localptr(String, channel) = String_value_none();
  localptr(String, clientid) = String_value_none();

  clientid = jsmn_lookup_string(jc, "clientid");
  if (String_is_none(clientid)) { fprintf(stderr, "%s: missing clientid\n", __func__); return; }
  
  channel = jsmn_lookup_string(jc, "channel");
  if (String_is_none(channel)) { fprintf(stderr, "%s: missing channel\n", __func__); return; }
  
  /* Verify valid channel. */
  if (!Regex_match(s(channel), "^[0-9]+[.][0-9]+$")) {
    fprintf(stderr, "%s: invalid channel\n", __func__);
    return;
  }

  set_client(clientid);

  stop_playback_programs(STOP_ALL);

  /* Start azap. */
  azap_cmd = String_sprintf("/platform/rpi/bin/azap.jsg -j azap.json -c azap.conf -q -r %s", s(channel));
  bgstart(s(azap_cmd), &playbackState.azap_pid);

  /* Start muxer. */
  mux_cmd = String_sprintf("/platform/rpi/bin/streammux");
  bgstart(s(mux_cmd), &playbackState.mux_pid);
  
  /* Start player. */
  unlink("/tmp/pb.in");
  unlink("/tmp/pb.out");
  player_cmd = String_sprintf("empty -p empty_omxplayer.pid "
			      "-i /tmp/pb.in -o /tmp/pb.out "
			      "-f /usr/bin/omxplayer tcp://localhost:5102 --layer 1 --lavfdopts probesize:250000");
  bgstart(s(player_cmd), &playbackState.player_pid);

  String_clear(&playbackState.title);
  playbackState.title = String_dup(channel);
  playbackState.playing = 1;
  playbackState.paused = 0;
  playbackState.recording = 0;
}

void command_play_url(JsmnContext * jc)
{
  localptr(String, player_cmd) = String_value_none();
  localptr(String, clientid) = String_value_none();
  localptr(String, url) = String_value_none();

  clientid = jsmn_lookup_string(jc, "clientid");
  if (String_is_none(clientid)) { fprintf(stderr, "%s: missing clientid\n", __func__); return;  }
  
  url = jsmn_lookup_string(jc, "url");
  if (String_is_none(url)) { fprintf(stderr, "%s: missing url\n", __func__); return; }

  /* Verify valid url */
  //if (!Regex_match((channel), "^...$")) {
  //fprintf(stderr, "%s: invalid channel\n", __func__);
  //goto out;
  //}
  
  set_client(clientid);

  stop_playback_programs(STOP_ALL);

  /* Start player. */
  unlink("/tmp/pb.in");
  unlink("/tmp/pb.out");
  player_cmd = String_sprintf("empty -p empty_omxplayer.pid"
			      " -i /tmp/pb.in -o /tmp/pb.out"
			      " -f livestreamer -np '/usr/bin/omxplayer --layer 1 --lavfdopts probesize:250000'"
			      " '%s'"
			      " best"
			      ,
			      s(url));
  // -i /tmp/pb.in -o /tmp/pb.out 
  bgstart(s(player_cmd), &playbackState.player_pid);

  String_clear(&playbackState.title);
  playbackState.title = String_dup(url); /* FIXME: Potential XSS vector here when it gets passed back in config? */
  playbackState.playing = 1;
  playbackState.paused = 0;
  playbackState.recording = 0;
}

void command_record_start(JsmnContext * jc)
{
  /*
   * Start recorder if mux running.
   */
  localptr(String, clientid) = String_value_none();
}

void command_record_stop(JsmnContext * jc)
{
  /*
   * Stop existing recorder.
   */
  localptr(String, clientid) = String_value_none();
}

void command_record_duration(JsmnContext * jc)
{
  /*
   * Start recorder if mux running.
   */
  localptr(String, clientid) = String_value_none();
  localptr(String, duration) = String_value_none();

  clientid = jsmn_lookup_string(jc, "clientid");
  clientid = jsmn_lookup_string(jc, "duration");
  if (String_is_none(clientid)) {  fprintf(stderr, "%s: missing clientid\n", __func__);  return;  }
}

void command_stop(JsmnContext * jc)
{
  /*
   * Stop existing player.
   * Leave  azap, mux, and record running.
   */
  localptr(String, clientid) = String_value_none();

  clientid = jsmn_lookup_string(jc, "clientid");
  if (String_is_none(clientid)) {  fprintf(stderr, "%s: missing clientid\n", __func__);  return; }

  if (String_eq(clientid, playbackState.clientid)) {
    stop_playback_programs(STOP_PLAYBACK);
  }
}

void command_pause(JsmnContext * jc)
{
  /*
   * Pause if possible.
   */
  localptr(String, clientid) = String_value_none();
}

void command_seek(JsmnContext * jc)
{
  /*
   * Seek if possible.
   */
  localptr(String, amount) = String_value_none();
  localptr(String, clientid) = String_value_none();
}

void command_snapshot(JsmnContext * jc)
{
  /*
   * Snapshot if possible.
   */
}

void command_bump(JsmnContext * jc)
{
  /* For testing. Possibly modify the sequence. */
  int seqdelta;
  if (jsmn_lookup_int(jc, "seqdelta", &seqdelta) == 0) {
    printf("playbackState.sequence: %d", playbackState.sequence);
    playbackState.sequence += seqdelta;
    printf(" -> %d\n", playbackState.sequence);
  }
}

void command_getstate(JsmnContext * jc)
{
  localptr(String, sequence) = String_value_none();
  int client_sequence;

  jc->flag1 = 0;

  sequence = jsmn_lookup_string(jc, "sequence");
  if (String_is_none(sequence)) {  fprintf(stderr, "%s: missing sequence\n", __func__);  return;  }
  client_sequence = atoi(s(sequence));

  fprintf(stderr, "\ngetstate ipc fd=%d\n", jc->fd1);
  if (client_sequence == playbackState.sequence) {
#ifdef PBPIPE
    playbackState.wake_count += 1;
#endif
#ifdef PBEVENTFD
    playbackState.wake_count += 1;
#endif

    /* Release lock. */
    Lock_release(&playbackState.lock);

    /* Block waiting for sequence update, or ipc socket to timeout. */
    sequence_wait(jc->fd1);
    fprintf(stderr, "done wait ipc fd=%d\n", jc->fd1);

    /* Grab lock and continue. */
    Lock_acquire(&playbackState.lock);
  }

  if (client_sequence != playbackState.sequence) {
    jc->result = String_sprintf("{"
				"\"sequence\":%d"
				",\"playing\":%d"
				",\"paused\":%d"
				",\"recording\":%d"
				",\"title\":\"%s\""
				",\"runtime\":\"%lld\""
				",\"current_time\":\"%lld\""
				",\"clientid\":\"%s\""
				"}\n", 
				playbackState.sequence,
				playbackState.playing,
				playbackState.paused,
				playbackState.recording,			  
				String_is_none(playbackState.title) ? "":s(playbackState.title),
				playbackState.run_time,
				playbackState.current_time,
				String_is_none(playbackState.clientid) ? "":s(playbackState.clientid)
				);
  }

}

static void * handle_ipc(void * ptr)
{
  int *pfd = ptr;
  int fd = *pfd;
  localptr(JsmnContext, jc) = JsmnContext_new();
  uint8_t * message = NULL;
  uint32_t message_length = 0;
  // String * result = String_value_none();

  int rc = cti_ipc_recv(fd, &message, &message_length, 2000);
  if (rc != 0) {
    goto out;
  }

  jc->js_str = String_from_uchar(message, message_length);
  jc->fd1 = fd;

  /* Most commands trigger a wake, clear this flag in handlers that should not. */
  jc->flag1 = 1;

  /* 
   * Example JSON command syntax,
   *  { "command":"play_qam" "channel":"..." }
   */

  static JsmnDispatchHandler commands[] = {
    { "play_qam", .handler = command_play_qam },
    { "play_url", .handler = command_play_url },
    { "record_start", .handler = command_record_start },
    { "record_stop",  .handler = command_record_stop  },
    { "record_duration", .handler = command_record_duration },
    { "stop", .handler = command_stop },
    { "pause", .handler = command_pause },
    { "seek", .handler = command_seek },
    { "snapshot", .handler = command_snapshot },
    { "bump", .handler = command_bump },
    { "getstate", .handler = command_getstate },
  };

  Lock_acquire(&playbackState.lock);
  // result = jsmn_dispatch((const char*)message, message_length, "command", commands, cti_table_size(commands));

  jsmn_dispatch(jc, "command", commands, cti_table_size(commands));

  if (!String_is_none(jc->result)) { 
    /* Successful query commands return a string. */
    cti_ipc_send(fd, (uint8_t *) s(jc->result), String_len(jc->result), 250);
  }

  if (jc->flag1) {
    /* Imperative commands do not return anything, but should trigger a config update.*/
    wake_config();
  }

  Lock_release(&playbackState.lock);

 out:
  if (message) {
    cti_ipc_free(&message, &message_length);
  }
  close(fd);
  free(pfd);
  return NULL;
}


static void accept_connection(int listenfd)
{
  struct sockaddr_in addr = {};
  socklen_t addrlen = sizeof(addr);
  int fd = accept(listenfd, (struct sockaddr *)&addr, &addrlen);
  if (fd == -1) {
    /* This is unlikely but possible.  If it happens, just clean up                          
       and continue. */
    perror("accept");
    return;
  }

  int *pfd = malloc(sizeof(*pfd)); *pfd = fd;

  pthread_t t;
  int rc = pthread_create(&t, NULL, handle_ipc, pfd);
  if (rc == 0) {
    pthread_detach(t);
  }
  else {
    perror("accept_connection:pthread_create");
  }
}


int main(int argc, char *argv[])
{
  int rc;

  playbackState.title = String_value_none();
  playbackState.clientid = String_value_none();
#ifdef PBINOTIFY
  playbackState.synchronization_path = String_sprintf("/tmp/playback-manager.%d.sync", getpid());
  playbackState.synchronization_file = fopen(s(playbackState.synchronization_path), "w");
  if (!playbackState.synchronization_file) {
    fprintf(stderr, "Failed to create %s\n", s(playbackState.synchronization_path));
    return 1;
  }
#endif

#ifdef PBEVENTFD
  if ((playbackState.wakefd = eventfd(0L, EFD_SEMAPHORE)) == -1) {
    perror("eventfd create");
    return 1;
  }
#endif

#ifdef PBPIPE
  if (pipe(playbackState.wakepipe) == -1) {
    perror("pipe");
    return 1;
  }
#endif

  char * base = getenv("HOME");
  if (!base) { base = ""; }  
  if (chdir(s(String_sprintf("%s/etc", base))) != 0) {
    fprintf(stderr, "working directory does not exist\n");
    return 1;
  }

  system("killall azap.jsg");
  system("killall omxplayer");

  listen_common lsc = {};
  lsc.port = 5101;
  rc = listen_socket_setup(&lsc);
  if (rc != 0) {
    goto out;
  }

  int done = 0;
  while (!done) {
    struct pollfd fds[2];
    fds[0].fd = lsc.fd;
    fds[0].events = POLLIN;

    if (poll(fds, 1, 1000) > 0) {
      if (fds[0].revents & POLLIN) {
	accept_connection(lsc.fd);
      }
    }
    /* Do things in between poll timeouts. */
    
  }

 out:
  return 0;
}
