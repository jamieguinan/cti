/*
 * Dynamically control a V4L2Capture instance based on captured video
 * frames.
 */
#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc, abs */
#include <string.h>             /* memcpy */
#include <math.h>               /* round */

#include "CTI.h"
#include "DynamicVideoControl.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void y420p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV420P };
static Input DynamicVideoControl_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = y420p_handler },
};

enum { OUTPUT_CONFIG };
static Output DynamicVideoControl_outputs[] = {
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

typedef struct {
  int Brightness;
  int Contrast;
  int Saturation;
  int Gain;
  int BackLightCompensation;
  int ExposureAbsolute;
  int LEDMode;
} VideoParams;

typedef struct {
  Instance i;
  VideoParams light;            /* Parameters for bright/daylight. */
  VideoParams dark;             /* Parameters for dark/nighttime. */
  VideoParams current;          /* Most recently passed parameters. */
  float current_level;
  int target_luma;              /* Desired average luma */
  int format_threshold;         /* For switching format between MJPG and YUYV */
  int kick;                     /* Force update */
  int fcount;
} DynamicVideoControl_private;

static Config config_table[] = {
  { "Brightness_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.Brightness) },
  { "Brightness_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.Brightness) },
  { "Contrast_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.Contrast ) },
  { "Contrast_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.Contrast ) },
  { "Saturation_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.Saturation ) },
  { "Saturation_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.Saturation ) },
  { "Gain_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.Gain ) },
  { "Gain_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.Gain ) },
  { "BackLightCompensation_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.BackLightCompensation ) },
  { "BackLightCompensation_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.BackLightCompensation ) },
  { "ExposureAbsolute_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.ExposureAbsolute ) },
  { "ExposureAbsolute_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.ExposureAbsolute ) },
  //{ "LEDMode_light", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, light.LEDMode ) },
  //{ "LEDMode_dark", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, dark.LEDMode ) },

  { "target_luma", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, target_luma ) },
  { "format_threshold", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, format_threshold) },
  { "kick", 0L, 0L, 0L, cti_set_int, offsetof(DynamicVideoControl_private, kick) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static int adjust(int left_limit, int right_limit, float level)
{
  float range = right_limit - left_limit;
  float value = left_limit + (range*level);
  return ((int)round(value));
}

static void y420p_handler(Instance *pi, void *msg)
{
  DynamicVideoControl_private *priv = (DynamicVideoControl_private *)pi;
  YUV420P_buffer *y420p = msg;
  int do_update = 0;
  int y, x;
  int delta = 10;                /* Only sample every Nth pixel in both directions. */
  int sample_count = 0;
  long sample_sum = 0;

  for (y=0; y < y420p->height; y += delta) {
    for (x=0; x < y420p->width; x += delta) {
      sample_sum += (y420p->y[y*y420p->width + x]);
      sample_count += 1;
    }
  }
  int luma = sample_sum / sample_count;

  priv->fcount += 1;
  if (priv->fcount % 30 == 0) {
    dpf("luma: %d\n", luma);
  }

  if (priv->fcount % 300 != 0) {
    goto out;
  }


  if (abs(luma - priv->target_luma) > 10) {
    do_update = 1;
  }
  else if (priv->kick) {
    do_update = 1;
    priv->kick = 0;
  }

  if (do_update && pi->outputs[OUTPUT_CONFIG].destination) {
    float delta = 0.025;         /* adjust levels by this amount */
    float level;                /* new level */
    if (luma > priv->target_luma) {
      /* Move bright scene limit */
      level = cti_max(priv->current_level - delta, 0.0);
    }
    else {
      /* Move toward dark scene limit */
      level = cti_min(priv->current_level + delta, 1.0);
    }

    printf("level %.3f -> %.3f\n", priv->current_level, level);

    if (level == priv->current_level) {
      /* Nothing to change. */
      goto out;
    }

    priv->current.Brightness = adjust(priv->light.Brightness, priv->dark.Brightness, level);
    priv->current.Contrast = adjust(priv->light.Contrast, priv->dark.Contrast, level);
    priv->current.Saturation = adjust(priv->light.Saturation, priv->dark.Saturation, level);
    priv->current.Gain = adjust(priv->light.Gain, priv->dark.Gain, level);
    priv->current.BackLightCompensation = adjust(priv->light.BackLightCompensation, priv->dark.BackLightCompensation, level);
    priv->current.ExposureAbsolute = adjust(priv->light.ExposureAbsolute, priv->dark.ExposureAbsolute, level);

    priv->current_level = level;
    char tmp[64];
#define xx(value) (sprintf(tmp, "%d", priv->current. value), printf("%s:%s\n", #value, tmp), tmp)
    PostData(Config_buffer_new("Brightness", xx(Brightness)), pi->outputs[OUTPUT_CONFIG].destination);
    PostData(Config_buffer_new("Contrast", xx(Contrast)), pi->outputs[OUTPUT_CONFIG].destination);
    PostData(Config_buffer_new("Saturation", xx(Saturation)), pi->outputs[OUTPUT_CONFIG].destination);
    PostData(Config_buffer_new("Gain", xx(Gain)), pi->outputs[OUTPUT_CONFIG].destination);
    PostData(Config_buffer_new("BackLight.Compensation", xx(BackLightCompensation)), pi->outputs[OUTPUT_CONFIG].destination);
    PostData(Config_buffer_new("Exposure.(Absolute)", xx(ExposureAbsolute)), pi->outputs[OUTPUT_CONFIG].destination);
  }

 out:
  YUV420P_buffer_release(y420p);
}

static void DynamicVideoControl_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void DynamicVideoControl_instance_init(Instance *pi)
{
  DynamicVideoControl_private *priv = (DynamicVideoControl_private *)pi;
  priv->current_level = 0.05;
}


static Template DynamicVideoControl_template = {
  .label = "DynamicVideoControl",
  .priv_size = sizeof(DynamicVideoControl_private),
  .inputs = DynamicVideoControl_inputs,
  .num_inputs = table_size(DynamicVideoControl_inputs),
  .outputs = DynamicVideoControl_outputs,
  .num_outputs = table_size(DynamicVideoControl_outputs),
  .tick = DynamicVideoControl_tick,
  .instance_init = DynamicVideoControl_instance_init,
};

void DynamicVideoControl_init(void)
{
  Template_register(&DynamicVideoControl_template);
}
