/* Lirc client.  IR remote control input for Linux.
 * NOTE: This is probably going to be obviated by LinuxInput.c */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Lirc.h"
#include "Keycodes.h"

#include <lirc/lirc_client.h>
#include <unistd.h>
#include <fcntl.h>

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input Lirc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_KEYCODE };
static Output Lirc_outputs[] = {
  [ OUTPUT_KEYCODE ] = { .type_label = "Keycode_message", .destination = 0L },
};

typedef struct {
  Instance i;
  int initialized;
  int fd;
  struct lirc_config *lircconfig;
} Lirc_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static struct {
  const char *keystring;
  int keycode;
} CodeMap[] = {
  {"KEY_0", CTI__KEY_0},
  {"KEY_1", CTI__KEY_1},
  {"KEY_2", CTI__KEY_2},
  {"KEY_3", CTI__KEY_3},
  {"KEY_4", CTI__KEY_4},
  {"KEY_5", CTI__KEY_5},
  {"KEY_6", CTI__KEY_6},
  {"KEY_7", CTI__KEY_7},
  {"KEY_8", CTI__KEY_8},
  {"KEY_9", CTI__KEY_9},
  {"KEY_TV", CTI__KEY_TV},
  {"KEY_VIDEO", CTI__KEY_VIDEO},
  {"KEY_MP3", CTI__KEY_MP3},
  {"KEY_MEDIA", CTI__KEY_MEDIA},
  {"KEY_RADIO", CTI__KEY_RADIO},
  {"KEY_OK", CTI__KEY_OK},
  {"KEY_VOLUMEDOWN", CTI__KEY_VOLUMEDOWN},
  {"KEY_VOLUMEUP", CTI__KEY_VOLUMEUP},
  {"KEY_CHANNELUP", CTI__KEY_CHANNELUP},
  {"KEY_CHANNELDOWN", CTI__KEY_CHANNELDOWN},
  {"KEY_PREVIOUS", CTI__KEY_PREVIOUS},
  {"KEY_MUTE", CTI__KEY_MUTE},
  {"KEY_PLAY", CTI__KEY_PLAY},
  {"KEY_PAUSE", CTI__KEY_PAUSE},
  {"KEY_RED", CTI__KEY_RED},
  {"KEY_GREEN", CTI__KEY_GREEN},
  {"KEY_YELLOW", CTI__KEY_YELLOW},
  {"KEY_BLUE", CTI__KEY_BLUE},
  {"KEY_RECORD", CTI__KEY_RECORD},
  {"KEY_STOP", CTI__KEY_STOP},
  {"KEY_MENU", CTI__KEY_MENU},
  {"KEY_EXIT", CTI__KEY_EXIT},
  {"KEY_UP", CTI__KEY_UP},
  {"KEY_DOWN", CTI__KEY_DOWN},
  {"KEY_LEFT", CTI__KEY_LEFT},
  {"KEY_RIGHT", CTI__KEY_RIGHT},
  {"KEY_NUMERIC_POUND", CTI__KEY_NUMERIC_POUND},
  {"KEY_NUMERIC_STAR", CTI__KEY_NUMERIC_STAR},
  {"KEY_FASTFORWARD", CTI__KEY_FASTFORWARD},
  {"KEY_REWIND", CTI__KEY_REWIND},
  {"KEY_BACK", CTI__KEY_BACK},
  {"KEY_FORWARD", CTI__KEY_FORWARD},
  {"KEY_HOME", CTI__KEY_HOME},
  {"KEY_POWER", CTI__KEY_POWER},
};


static void handle_code(Instance *pi, const char *code)
{
  /* Map code to output token and pass along to output. */
  int i;
  for (i=0; i < table_size(CodeMap); i++) {
    if (streq(code, CodeMap[i].keystring)) {
      printf("code %s -> %d\n",  CodeMap[i].keystring, CodeMap[i].keycode);
      if (pi->outputs[OUTPUT_KEYCODE].destination) {
	PostData(Keycode_message_new(CodeMap[i].keycode),
		 pi->outputs[OUTPUT_KEYCODE].destination);
      }
      break;
    }
  }
}


static void Lirc_tick(Instance *pi)
{
  Handler_message *hm;
  Lirc_private *priv = (Lirc_private *)pi;
  int rc;
  int do_sleep = 1;
  int verbose = 1;

  if (!priv->initialized) {
    priv->fd = lirc_init("CTI", verbose);
    if (priv->fd == -1) {
      perror("lirc_init");
    }
    else {
      /* Set non-blocking so we can roll along if nothing is happening. */
      long value = O_NONBLOCK;
      fcntl(priv->fd, F_SETFL, value);

      /* I learned this by experimentation and a gdb session:
	 "/etc/lirc/lircd.conf" is read by lircd, and the entries in
	 the "begin codes" section are returned by lirc_nextcode.
	 lirc_readconfig() is used for client configuration, and does
	 not even parse the "begin codes" section.  A separate
	 configuration can be done for client code, see for example
	 "/usr/share/lirc/remotes/pcmak/lircrc.pcmak" */
      rc = lirc_readconfig("lirc-user.conf", &priv->lircconfig, NULL);
      if (rc == -1) {
	perror("lirc_readconfig");
      }
    }
    priv->initialized = 1;	/* set this even if failed. */
  }

  if (priv->fd != -1) {
    char *code;
    rc = lirc_nextcode(&code);
    if (code) {
      printf("code %s\n", code);
      do_sleep = 0;

      if (0) {
	/* I could only get every odd-numbered lirc_code2charprog call
	   to return a valid string.  Other calls returned null or
	   invalid pointer. */
	char *string = 0L;
	char *prog = 0L;
	rc = lirc_code2charprog(priv->lircconfig, code, &string, &prog);
	printf("rc=%d string=%s prog=%s\n", rc,
	       string? string: "(nil)",
	       prog? prog: "(nil)"
	       );
	//if (string && string[0]) {
	//free(string);
	//}
      }

      if (1) {
	/* So instead I parse it out manually.  This only works for my
	   particular configuration, using a Hauppauge remote with a
	   pcHDTV card using devinput driver.  See sully/notes.txt for
	   how I set that up.

            0004000400000014 00 KEY_UP /tmp/lirc.txt
	*/
	char *pkey = strstr(code, "KEY_");
	if (!pkey) {
	  goto nevermind;
	}

	char *space = strchr(pkey, ' ');
	if (!space) {
		  goto nevermind;
	}

	*space = 0;
	handle_code(pi, pkey);
	*space = ' ';		/* restore */

      nevermind: ;
      }

      free(code);
    }
  }

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
    do_sleep = 0;
  }

  if (do_sleep) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/10}, NULL);
  }

  pi->counter++;
}

static void Lirc_instance_init(Instance *pi)
{
  // Lirc_private *priv = (Lirc_private *)pi;
}


static Template Lirc_template = {
  .label = "Lirc",
  .priv_size = sizeof(Lirc_private),
  .inputs = Lirc_inputs,
  .num_inputs = table_size(Lirc_inputs),
  .outputs = Lirc_outputs,
  .num_outputs = table_size(Lirc_outputs),
  .tick = Lirc_tick,
  .instance_init = Lirc_instance_init,
};

void Lirc_init(void)
{
  Template_register(&Lirc_template);
}
