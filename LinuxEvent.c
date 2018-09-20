/* Linux input event handling. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include <sys/types.h>		/* open */
#include <sys/stat.h>		/* open */
#include <fcntl.h>		/* open */
#include <unistd.h>		/* close, read */

#include <linux/input.h>	/* event structures */

#include "CTI.h"
#include "LinuxEvent.h"
#include "Keycodes.h"

const char * type_strings[] = {
  [EV_SYN] = "EV_SYN",
  [EV_KEY] = "EV_KEY",
  [EV_REL] = "EV_REL",
  [EV_ABS] = "EV_ABS",
  [EV_MSC] = "EV_MSC",
};

static const char * ev_type_string(uint16_t type)
{
  if (type <= cti_table_size(type_strings) && type_strings[type]) {
    return type_strings[type];
  }
  else {
    return "unknown";
  }
}

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input LinuxEvent_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_KEYCODE };
static Output LinuxEvent_outputs[] = {
  [ OUTPUT_KEYCODE ] = { .type_label = "Keycode_msg", .destination = 0L },
};

typedef struct {
  Instance i;
  int grab;			/* Set before opening to grab device on open. */
  int devfd;
  String * devpath;
} LinuxEvent_private;


static void close_device(LinuxEvent_private *priv)
{
  close(priv->devfd);
  priv->devfd = -1;
  String_set(&priv->devpath, "");
}


static int set_device(Instance *pi, const char *value)
{
  LinuxEvent_private *priv = (LinuxEvent_private *)pi;
  int rc;

  if (priv->devfd != -1) {
    close_device(priv);
  }

  priv->devfd = open(value, O_RDONLY);
  if (priv->devfd == -1) {
    perror(value);
    return 1;
  }

  String_set(&priv->devpath, value);
  if (priv->grab) {
    rc = ioctl(priv->devfd, EVIOCGRAB, 1 /* atoi(value) */);
    if (rc != 0) {
      perror("EVIOCGRAB");
    }
    else {
      printf("Grabbed %s\n", s(priv->devpath));
    }
  }

  return 0;
}


static Config config_table[] = {
  { "device",     set_device, 0L, 0L},
  { "grab",       0L, 0L, 0L, cti_set_int, offsetof(LinuxEvent_private, grab) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void handle_next_event(Instance *pi)
{
  /* FIXME: This blocks once enabled, should probably use a poll timeout
     to let config events happen once in a while. */
  LinuxEvent_private *priv = (LinuxEvent_private *)pi;
  struct input_event ev;
  int n;

  n = read(priv->devfd, &ev, sizeof(ev));
  if (n != sizeof(ev)) {
    fprintf(stderr, "error reading %s, closing device\n", s(priv->devpath));
    close_device(priv);
    return;
  }

  // printf("%s code=%d value=%d\n", ev_type_string(ev.type), ev.code, ev.value);
  if (ev.type == EV_KEY && ev.value == 1) {
    int cti_key = Keycode_from_linux_event(ev.code);
    printf("%s code=%d (transalted=%d) value=%d\n", ev_type_string(ev.type), ev.code, cti_key,
	   ev.value);

    if (pi->outputs[OUTPUT_KEYCODE].destination) {
      PostData(Keycode_message_new(cti_key), pi->outputs[OUTPUT_KEYCODE].destination);
    }

  }

}


static void LinuxEvent_tick(Instance *pi)
{
  LinuxEvent_private *priv = (LinuxEvent_private *)pi;
  Handler_message *hm;
  int waitflag = 1;

  if (priv->devfd != -1) {
    waitflag = 0;
    handle_next_event(pi);
  }

  hm = GetData(pi, waitflag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void LinuxEvent_instance_init(Instance *pi)
{
  LinuxEvent_private *priv = (LinuxEvent_private *)pi;
  priv->devfd = -1;
}


static Template LinuxEvent_template = {
  .label = "LinuxEvent",
  .priv_size = sizeof(LinuxEvent_private),
  .inputs = LinuxEvent_inputs,
  .num_inputs = table_size(LinuxEvent_inputs),
  .outputs = LinuxEvent_outputs,
  .num_outputs = table_size(LinuxEvent_outputs),
  .tick = LinuxEvent_tick,
  .instance_init = LinuxEvent_instance_init,
};

void LinuxEvent_init(void)
{
  Template_register(&LinuxEvent_template);
}
