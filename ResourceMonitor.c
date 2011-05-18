/* Resource usage monitor. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include <sys/time.h>
#include <sys/resource.h>

#include "Template.h"
#include "ResourceMonitor.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input ResourceMonitor_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output ResourceMonitor_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  unsigned int period_ms;
  long rss_limit;
} ResourceMonitor_private;

static int set_period_ms(Instance *pi, const char *value)
{
  ResourceMonitor_private *priv =  pi->data;
  priv->period_ms = atoi(value);
  return 0;
}

static int set_rss_limit(Instance *pi, const char *value)
{
  ResourceMonitor_private *priv =  pi->data;
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
  ResourceMonitor_private *priv =  pi->data;
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

  rc = getrusage(RUSAGE_SELF, &usage);
  if (rc == 0) {
    // printf("ru_maxrss=%ld\n", usage.ru_maxrss);
    if (priv->rss_limit && priv->rss_limit > usage.ru_maxrss) {
      fprintf(stderr, "rss_limit exceded!\n");
      exit(1);
    }
  }

  pi->counter++;
}

static void ResourceMonitor_instance_init(Instance *pi)
{
  ResourceMonitor_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template ResourceMonitor_template = {
  .label = "ResourceMonitor",
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
