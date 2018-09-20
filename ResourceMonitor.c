/* Resource usage monitor. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include <sys/time.h>
#include <sys/resource.h>

#include "CTI.h"
#include "ResourceMonitor.h"
#include "dpf.h"
#include "File.h"
#include "localptr.h"

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
  Instance i;
  unsigned int period_ms;
  int rss_limit;               /* in KB */
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

static void _shutdown_(void)
{
  CTI_pending_messages();
  mdump();
  exit(1);
}


static void ResourceMonitor_tick(Instance *pi)
{
  ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
  Handler_message *hm;
  int rss_value;
  int i, end;

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (priv->period_ms) {
    nanosleep(&(struct timespec) { .tv_sec = priv->period_ms/1000 , .tv_nsec =  (priv->period_ms%1000) *1000000 }, NULL);
  }
  else {
    nanosleep(&(struct timespec){.tv_sec = 1, .tv_nsec = 0}, NULL);
  }

  localptr(String, data) = File_load_text(S("/proc/self/status"));
  i = String_find(data, 0, "VmRSS:", &end);
  if (i > 0) {
    String_parse_int(data, end+1, &rss_value);
    dpf("VmRSS %d\n", rss_value);
    if (priv->rss_limit && rss_value > priv->rss_limit)   {
      fprintf(stderr, "%s: rss_limit exceded (%d > %d)!\n", __func__,
              rss_value, priv->rss_limit);
      _shutdown_();
    }
  }

  pi->counter++;
}

static void ResourceMonitor_instance_init(Instance *pi)
{
  // ResourceMonitor_private *priv = (ResourceMonitor_private *)pi;
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
