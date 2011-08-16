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

#define cti_min(a, b)  ( (a) < (b) ? (a) : (b) )
#define cti_max(a, b)  ( (a) > (b) ? (a) : (b) )

static void Config_handler(Instance *pi, void *msg);
static void gray_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_GRAY, INPUT_422P };
static Input FaceTracker_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_GRAY ] = { .type_label = "GRAY_buffer", .handler = gray_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_POSITION, OUTPUT_GRAY, OUTPUT_RGB3, OUTPUT_422P };
static Output FaceTracker_outputs[] = {
  [ OUTPUT_POSITION ] = { .type_label = "Position_msg", .destination = 0L },
  /* The image outputs are for tuning and debugging. */
  [ OUTPUT_GRAY ] = { .type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_422P ] = { .type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  float x;
  float y;
} coord2d;


enum { 
  FACETRACKER_BACKGROUND_WAITING_LEFT,
  FACETRACKER_BACKGROUND_WAITING_RIGHT,
  FACETRACKER_BACKGROUND_READY,
};

enum { 
  FT_INIT,
  FT_BLINK_1,
  FT_BLINK_2,
  FT_BLINK_3,
  FT_TRACKING,
};

#define FIR_COUNT 8

typedef struct {
  coord2d lefteye, righteye, leftnostril, rightnostril, nose, mouth, leftear, rightear;
  float confidence;
  coord2d p1, p2, p3;

  int state;
  Gray_buffer *background;

  Gray32_buffer *sum;
  Gray_buffer *fir[FIR_COUNT];
  int fir_index;

  Gray_buffer *iir_last;
  float iir_decay;
  int chop;
  int search_ready;

} FaceTracker_private;

static int set_iir_decay(Instance *pi, const char *value)
{
  FaceTracker_private *priv = pi->data;
  priv->iir_decay = atof(value);
  return 0;
}

static int set_chop(Instance *pi, const char *value)
{
  FaceTracker_private *priv = pi->data;
  priv->chop = atoi(value);
  return 0;
}

static Config config_table[] = {
  { "iir_decay",    set_iir_decay, 0L, 0L },
  { "chop",    set_chop, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void analysis_00(FaceTracker_private *priv, Gray_buffer *gray)
{
  /* confirm previous measurements, and adjust with score */
  /* Check for "wink" to reset position offsets to 0. */

  /* find eyes, and angle */
  /* find mouth */
  /* find nose */
  /* estimate skin tone? would require a color buffer */
  /* measure visible face on sides of eyes */

  /* Hm, finding my Nostrils might be easy. */

  /* Idea: sit still and blink eyes to start.  Track eyes and nostrils. */
  
  
}


static void analysis_001(FaceTracker_private *priv, Gray_buffer *gray)
{
  /* Idea: sit still and blink eyes to start.  Track eyes and nostrils. */
  /* Track starting from previous measurements.  Score? */
  int prev_index = (priv->fir_index + FIR_COUNT - 1) % FIR_COUNT;
  int prev2_index = (priv->fir_index + FIR_COUNT - 2) % FIR_COUNT;
  int num_pixels = gray->data_length;
  int i;

  /* Allocate sum buffer if necessary. */
  if (!priv->sum) {
    priv->sum = Gray32_buffer_new(gray->width, gray->height);
  }

  /* Save a copy of the frame. */
  if (priv->fir[priv->fir_index] == 0) {
    priv->fir[priv->fir_index] = Gray_buffer_new(gray->width, gray->height);
  }
  memcpy(priv->fir[priv->fir_index]->data, gray->data, gray->data_length);

  /* Do calculations only if have 2 buffers to work with. */
  if (priv->fir[prev_index] == 0) {
    goto out;
  }

  /* Absolute value difference between frames. */
  for (i=0; i < num_pixels; i++) {
    //priv->sum->data[i] += (priv->fir[priv->fir_index]->data[i] + 256 - 
    //priv->fir[prev_index]->data[i]);
    priv->sum->data[i] += abs((int)(priv->fir[priv->fir_index]->data[i]) - 
			   (int)(priv->fir[prev_index]->data[i]));
  }

  /* Clean up oldest frame's contribution. */
  if (priv->fir[prev2_index] == 0) {
    goto out;
  }
  for (i=0; i < num_pixels; i++) {
    //priv->sum->data[i] -= (priv->fir[prev_index]->data[i] + 256 -
    //priv->fir[prev2_index]->data[i]);
    priv->sum->data[i] -= abs((int)(priv->fir[prev_index]->data[i]) -
			      (int)(priv->fir[prev2_index]->data[i]));
    
  }

  /* Diagnostic output: */
  for (i=0; i < num_pixels; i++) {
    uint8_t sample = priv->sum->data[i];
    sample /= 2;
    gray->data[i] = cti_min(255, sample * sample);
  }

 out:
  priv->fir_index += 1;
  priv->fir_index %= FIR_COUNT;
}

static void analysis_002(FaceTracker_private *priv, Gray_buffer *gray, RGB3_buffer *rgb)
{
  /* Idea: sit still and blink eyes to start.  Track eyes and nostrils. */
  /* Track starting from previous measurements.  Score? */
  int num_pixels = gray->data_length;
  int i;
  int ii;
  int yy, xx;
  int dx;
  uint32_t localsum;
  int x, y;
  int y_step = 0;
  int x_step = 0;
  struct {
    int x;
    int y;
    uint32_t sum;
  } maximums[2] = {};

  /* Allocate sum buffer if necessary. */
  if (!priv->sum) {
    priv->sum = Gray32_buffer_new(gray->width, gray->height);
  }

  /* Do calculations only if have 2 buffers to work with. */
  if (priv->iir_last == 0) {
    priv->iir_last = Gray_buffer_new(gray->width, gray->height);
    goto postcalc;
  }

  /* Do IIR filter adding squared absolute value difference between frames. */
  int sum = 0;
  for (i=0; i < num_pixels; i++) {
    uint32_t pixel = priv->sum->data[i];
    pixel *= priv->iir_decay;
    int d = abs((int)(gray->data[i]) - (int)(priv->iir_last->data[i]));
    if (d < priv->chop) { 
      d = 0; 
    }
    else {
      d = d * d;
    }
    pixel += d;
    priv->sum->data[i] = pixel;
    sum += pixel;
  }

  if (sum < num_pixels * 3/2) {
    priv->search_ready = 1;
    printf("->Search Ready!\n");
  }
  else {
    goto postcalc;
  }

  /* The steps here are approximate dimensions of eyes relative to camera field of view. */
  y_step = priv->sum->height / 35;
  x_step = priv->sum->width / 22;
  memset(&maximums, 0, sizeof(maximums));
  
  /* Search only center portion of screen. */
  for (y=priv->sum->height/4; y < priv->sum->height*3/4; y += 1) {
    for (x=priv->sum->width/3; x <  priv->sum->width*2/3; x += 1) {
      /* Search for contiguous area with maximum sum, which should correspond
	 to one of the eyes being blinked. */
      localsum = 0;
      for (yy = 0; yy < y_step; yy++) {
	for (xx = 0; xx < x_step; xx++) {
	  int value = priv->sum->data[priv->sum->width*(yy+y) + (x+xx)];
	  localsum += value;
	}
      }
      if (localsum > maximums[0].sum) {
	//printf("localsum=%d  %d %d\n", localsum,  maximums[0].sum, maximums[1].sum);
	maximums[0].sum = localsum;
	maximums[0].x = x;
	maximums[0].y = y;
      }
    }
  }
  
  /* Search for other eye, either to left or right */
  for (y=maximums[0].y, dx = -1 ; dx <= 1; dx += 2) {
    for (x=maximums[0].x + (dx*x_step); (x > 0) && (x < priv->sum->width - x_step); x += dx) {
      /* Search for contiguous area with maximum sum, which should correspond the other eye. */
      localsum = 0;
      for (yy = 0; yy < y_step; yy++) {
	for (xx = 0; xx < x_step; xx++) {
	  int value = priv->sum->data[priv->sum->width*(yy+y) + (x+xx)];
	  localsum += value;
	}
      }
      /* Sum should be at least 1/3 of other sum. */
      if (localsum > maximums[1].sum && localsum > (maximums[0].sum * 1 / 3 )) {
	maximums[1].sum = localsum;
	maximums[1].x = x;
	maximums[1].y = y;
      }
    }
  }
  
  if (maximums[1].sum) {
    for (ii=0; ii < 2; ii++) {
      printf("maximums[%d] (%d, %d): %d\n",  
	     ii, maximums[ii].x,  maximums[ii].y,  maximums[ii].sum);
    }
  }

  if (maximums[1].sum) {
    /* Found possible eye match, now search for other features. */
    /* Then take a "template" of the face, morph it for
       up/down/left/right and store the patterns for later
       matching. */
    /* Or find mouth, nostrils, cheek patches and use them for calculations. */
  }
  
  
 postcalc:
  /* Save a copy of the frame. */
  memcpy(priv->iir_last->data, gray->data, gray->data_length);
  
  /* Replace grey image with diagnostic output. */
  for (i=0; i < num_pixels; i++) {
    uint32_t pixel = priv->sum->data[i];
    gray->data[i] = cti_min(255, pixel);
  }


  /* Draw boxes around the maximum regions, if 2 were found. */
  if (maximums[1].sum) {
    for (ii=0; ii < 2; ii++) {
      for (yy=maximums[ii].y; yy < maximums[ii].y+y_step; yy++) {
	gray->data[gray->width*yy + maximums[ii].x] = 255;
	gray->data[gray->width*yy + maximums[ii].x+x_step] = 255;
      }
      
    
      for (xx=maximums[ii].x; xx < maximums[ii].x+x_step; xx++) {
	if (xx < 0) xx = 0;
	gray->data[gray->width*maximums[ii].y + xx] = 255;
	gray->data[gray->width*(maximums[ii].y+y_step) + xx] = 255;
      }
    }
  }
}


static void analysis_01(FaceTracker_private *priv, Y422P_buffer *y422p)
{
#if 0
  /* Locate 3 points matching the selected color.  Calculate values to
     pass to XPlaneControl. */

  int points;
  int x, y;
  int x_step;
  int y_step;
  int height;
  int width;
  int target_size_x;
  int target_size_y;
  
  for (y = (0 + y_step); y < (height - target_size_y); y += y_step) {
    for (x = (0 + x_step); x < (width - target_size_x); x += x_step) {
      if (color_match(x, y)) {
	/* Scan region around (x,y), count matching pixels and take average of
	   matching coordinates. */
	/* If results are acceptable, store the "point".  */
	puts("ohai");
      }
    }
  }
#endif
}


static void gray_handler(Instance *pi, void *msg)
{
  FaceTracker_private *priv = pi->data;
  Gray_buffer *gray = msg;

  /* Position message will contain 3D coordinate offset and 3D rotation offset. */

  // analysis_00(priv, gray);
  if (priv->state == 0) {
    RGB3_buffer *rgb = NULL;
    if (pi->outputs[OUTPUT_GRAY].destination) {
      analysis_002(priv, gray, rgb);
    }
  }
  else if (priv->state == 1) {
    
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    PostData(gray, pi->outputs[OUTPUT_GRAY].destination);
  }
  else {
    Gray_buffer_discard(gray);
  }
}


static void y422p_handler(Instance *pi, void *msg)
{
  FaceTracker_private *priv = pi->data;
  Y422P_buffer *y422p = msg;

  /* Position message will contain 3D coordinate offset and 3D rotation offset. */

  analysis_01(priv, y422p);

  if (pi->outputs[OUTPUT_422P].destination) {
    PostData(y422p, pi->outputs[OUTPUT_422P].destination);
  }
  else {
    Y422P_buffer_discard(y422p);
  }
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
