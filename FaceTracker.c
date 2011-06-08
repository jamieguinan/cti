/* 
 * Face tracker.  I don't know how hard this is going to be to
 * implement.  I initially want it for controlling the cockpit
 * view in X-Plane.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "FaceTracker.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void gray_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_GRAY };
static Input FaceTracker_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY] = { .type_label = "GRAY_buffer", .handler = gray_handler },
};

//enum { /* OUTPUT_... */ };
static Output FaceTracker_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  float x;
  float y;
} coord2d;

typedef struct {
  coord2d lefteye, righteye, nose, mouth, leftear, rightear;
  float confidence;
} FaceTracker_private;

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void gray_handler(Instance *pi, void *msg)
{
  FaceTracker_private *priv = pi->data;
  Gray_buffer *gray = msg;
  
  

  Gray_buffer_discard(gray);
}


static void FaceTracker_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void FaceTracker_instance_init(Instance *pi)
{
  FaceTracker_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template FaceTracker_template = {
  .label = "FaceTracker",
  .inputs = FaceTracker_inputs,
  .num_inputs = table_size(FaceTracker_inputs),
  .outputs = FaceTracker_outputs,
  .num_outputs = table_size(FaceTracker_outputs),
  .tick = FaceTracker_tick,
  .instance_init = FaceTracker_instance_init,
};

void FaceTracker_init(void)
{
  Template_register(&FaceTracker_template);
}
