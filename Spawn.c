/* Allow spawning a child process in response to an event. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* fork */
#include <sys/types.h>
#include <sys/wait.h>

#include "CTI.h"
#include "Spawn.h"
#include "Keycodes.h"

static void Config_handler(Instance *pi, void *msg);
static void Keycode_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_KEYCODE };
static Input Spawn_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_message", .handler = Keycode_handler },
};

//enum { /* OUTPUT_... */ };
static Output Spawn_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  String *child_cmdline;
  int trigger_key;
  int need_retsart;
  pid_t child_pid;
} Spawn_private;


static int set_cmdline(Instance *pi, const char *value)
{
  Spawn_private *priv = (Spawn_private *)pi;
  if (priv->child_cmdline) {
    String_free(&priv->child_cmdline);
  }
  priv->child_cmdline = String_new(value);
  return 0;
}


static int set_trigger_key(Instance *pi, const char *value)
{
  Spawn_private *priv = (Spawn_private *)pi;
  priv->trigger_key = Keycode_from_string(value);
  return 0;
}


static Config config_table[] = {
  { "cmdline",     set_cmdline, 0L, 0L },
  { "trigger_key", set_trigger_key, 0L, 0L },
};


static void Keycode_handler(Instance *pi, void *msg)
{
  Spawn_private *priv = (Spawn_private *)pi;
  Keycode_message *km = msg;

  if (km->keycode == priv->trigger_key && priv->child_pid == -1) {
    priv->child_pid = fork();
    if (priv->child_pid == 0) {
      /* Child. */
      /* FIXME: This only allows for a single command with no parameters. */
      execl(priv->child_cmdline->bytes, priv->child_cmdline->bytes, NULL);
      perror("execl");
      exit(1);
    }
    else if (priv->child_pid > 0) {
      /* Parent. */
    }
  }

  Keycode_message_cleanup(&km);
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Spawn_tick(Instance *pi)
{
  Handler_message *hm;
  Spawn_private *priv = (Spawn_private *)pi;
  
  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }
  else {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);
  }

  if (priv->child_pid > 0) {
    int status;
    pid_t pid =  waitpid(priv->child_pid, &status, WNOHANG);
    if (pid > 0 && pid == priv->child_pid) {
      priv->child_pid = -1;
    }
  }
  
  pi->counter++;
}

static void Spawn_instance_init(Instance *pi)
{
  Spawn_private *priv = (Spawn_private *)pi;
  
  priv->trigger_key = -1;
  priv->child_pid = -1;
}


static Template Spawn_template = {
  .label = "Spawn",
  .priv_size = sizeof(Spawn_private),
  .inputs = Spawn_inputs,
  .num_inputs = table_size(Spawn_inputs),
  .outputs = Spawn_outputs,
  .num_outputs = table_size(Spawn_outputs),
  .tick = Spawn_tick,
  .instance_init = Spawn_instance_init,
};

void Spawn_init(void)
{
  Template_register(&Spawn_template);
}
