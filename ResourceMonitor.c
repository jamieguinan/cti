/* Resource usage monitor. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include <sys/time.h>
#include <sys/resource.h>

#include "CTI.h"
#include "ResourceMonitor.h"
#include "Cfg.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ResourceMonitor_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output ResourceMonitor_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

enum {
  RESOURCE_MONITOR_MODE_NONE, 
  RESOURCE_MONITOR_MODE_RUSAGE, 
  RESOURCE_MONITOR_MODE_PROCSELFSTAT
};

typedef struct {
  Instance i;
  int mode;
  unsigned int period_ms;
  long rss_limit;
  int procselfstat_field;
} ResourceMonitor_private;

static int set_period_ms(Instance *pi, const char *value)
{
  ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
  priv->period_ms = atoi(value);
  return 0;
}

static int set_rss_limit(Instance *pi, const char *value)
{
  ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
  priv->rss_limit = atoi(value);
  return 0;
}

static Config config_table[] = {
  { "period_ms", set_period_ms, 0L, 0L },
  { "rss_limit", set_rss_limit, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void ResourceMonitor_tick(Instance *pi)
{
  ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
  Handler_message *hm;
  int rc;
  struct rusage usage;

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  if (priv->period_ms) {
    nanosleep(&(struct timespec) { .tv_sec = priv->period_ms/1000 , .tv_nsec =  (priv->period_ms%1000) *1000000 }, NULL);
  }
  else {
    nanosleep(&(struct timespec){.tv_sec = 1, .tv_nsec = 0}, NULL);
  }

  if (priv->mode == RESOURCE_MONITOR_MODE_RUSAGE) {
    // rc = getrusage(RUSAGE_SELF, &usage);
    int id = (pi->counter % 3) - 1;
    rc = getrusage(id, &usage);  
    if (rc == 0) {
      if (cfg.verbosity) {
	printf("ru_maxrss=%ld\n", usage.ru_maxrss);
	printf("ixrss=%ld\n", usage.ru_ixrss);
      }
      if (priv->rss_limit && usage.ru_maxrss > priv->rss_limit) {
	fprintf(stderr, "%s: rss_limit exceded (%ld > %ld)!\n", __func__, usage.ru_maxrss, priv->rss_limit);
	exit(1);
      }
    }
  }

  else if (priv->mode == RESOURCE_MONITOR_MODE_PROCSELFSTAT) {
    int i=0;
    char *p;
    String *data = File_load_text(S("/proc/self/stat"));
    p = data->bytes;
    while (*p) {
      if (*p == ' ') {
	i++;
	if (i == priv->procselfstat_field) {
	  long value;
	  int n = sscanf(p+1, "%ld", &value);
	  if (n == 1) {
	    long rss_value = (value / 1024);
	    if (priv->rss_limit && rss_value > priv->rss_limit)   {
	      fprintf(stderr, "%s: rss_limit exceded (%ld > %ld)!\n", __func__, 
		      rss_value, priv->rss_limit);
	      exit(1);
	    }
	    if (cfg.verbosity) {
	      printf("VSZ or RSS %ld\n", rss_value);
	    }
	  }
	  break;
	}
      }
      p++;
    }
    String_free(&data);
  }
    
  pi->counter++;
}

static void ResourceMonitor_instance_init(Instance *pi)
{
  ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
  
  priv->mode = RESOURCE_MONITOR_MODE_PROCSELFSTAT;
  priv->procselfstat_field = 22;
}


static Template ResourceMonitor_template = {
  .label = "ResourceMonitor",
  .priv_size = sizeof(ResourceMonitor_private),
  .inputs = ResourceMonitor_inputs,
  .num_inputs = table_size(ResourceMonitor_inputs),
  .outputs = ResourceMonitor_outputs,
  .num_outputs = table_size(ResourceMonitor_outputs),
  .tick = ResourceMonitor_tick,
  .instance_init = ResourceMonitor_instance_init,
};

void ResourceMonitor_init(void)
{
  Template_register(&ResourceMonitor_template);
}
