/* 
 * Filter operations, mostly operating on the Y channel for YCrCb images.
 * I didn't like this module for a while, but it has sort of grown on me.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422p_handler(Instance *pi, void *msg);
static void RGB3_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV422P, INPUT_RGB3 };
static Input VFilter_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = Y422p_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
};

enum { OUTPUT_YUV422P, OUTPUT_RGB3 };
static Output VFilter_outputs[] = {
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  int left_right_crop;
  int top_crop;
  int bottom_crop;
  int trim;
  int field_split;
  int linear_blend;
  int y3blend;
  int adaptive3point;
  int horizontal_filter[5];
  int horizontal_filter_divisor;
  int horizontal_filter_enabled;
} VFilter_private;


static int set_left_right_crop(Instance *pi, const char *value)
{
  VFilter_private *priv = (VFilter_private *)pi;
  
  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid left_right_crop value %d\n", tmp);
    return -1;
  }
  else {
    priv->left_right_crop = tmp;
  }
  return 0;
}

static int set_bottom_crop(Instance *pi, const char *value)
{
  VFilter_private *priv = (VFilter_private *)pi;
  
  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid bottom_crop value %d\n", tmp);
    return -1;
  }
  else {
    priv->bottom_crop = tmp;
  }
  return 0;
}

static int set_linear_blend(Instance *pi, const char *value)
{
  VFilter_private *priv = (VFilter_private *)pi;

  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid linear_blend value %d\n", tmp);
    return -1;
  }
  else {
    priv->linear_blend = tmp;
  }
  return 0;
}


static int set_trim(Instance *pi, const char *value)
{
  VFilter_private *priv = (VFilter_private *)pi;

  int tmp =  atoi(value);
  if (tmp < 0) {
    fprintf(stderr, "invalid trim value %d\n", tmp);
    return -1;
  }
  else {
    priv->trim = tmp;
  }
  return 0;
}


static int set_horizontal_filter(Instance *pi, const char *value)
{
  VFilter_private *priv = (VFilter_private *)pi;
  int i;
  int n = sscanf(value,
		 "%d,%d,%d,%d,%d",
		 &priv->horizontal_filter[0],
		 &priv->horizontal_filter[1],
		 &priv->horizontal_filter[2],
		 &priv->horizontal_filter[3],
		 &priv->horizontal_filter[4]);

  if (n == 1 && priv->horizontal_filter[0] == 0) {
    fprintf(stderr, "%s: horizontal filter disabled\n", __func__);
    return 0;
  }

  if (n != 5) {
    fprintf(stderr, "%s: expected 5 comma-separated values\n", __func__);
    priv->horizontal_filter_enabled = 0;
    return -1;
  }

  priv->horizontal_filter_divisor = 0;
  fprintf(stderr, "\n(");
  for (i=0; i < 5; i++) {
    priv->horizontal_filter_divisor += priv->horizontal_filter[i];
    fprintf(stderr, "%s%d", 
	    (i == 0 ? "" : ","),
	    priv->horizontal_filter[i]);
  }

  if (priv->horizontal_filter_divisor < 0) {
    /* Use absolute value. */
    priv->horizontal_filter_divisor = -priv->horizontal_filter_divisor;
  }
  else if (priv->horizontal_filter_divisor == 0) {
    /* Avoid divide-by-zero, and get neat effect. */
    priv->horizontal_filter_divisor = 1;
  }

  fprintf(stderr, ") / %d\n", priv->horizontal_filter_divisor);

  priv->horizontal_filter_enabled = 1;

  return 0;
}


static Config config_table[] = {
  { "left_right_crop",  set_left_right_crop, 0L, 0L },
  { "top_crop",    0L, 0L, 0L, cti_set_int, offsetof(VFilter_private, top_crop) },
  { "bottom_crop",    set_bottom_crop, 0L, 0L },
  { "linear_blend",  set_linear_blend, 0L, 0L },
  { "y3blend",  0L, 0L, 0L, cti_set_int, offsetof(VFilter_private, y3blend) },
  { "trim",  set_trim, 0L, 0L },
  { "field_split",  0L, 0L, 0L, cti_set_int, offsetof(VFilter_private, field_split) },
  { "a3p",  0L, 0L, 0L, cti_set_int, offsetof(VFilter_private, adaptive3point) },
  { "horizontal_filter", set_horizontal_filter, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void single_121_linear_blend(uint8_t *data_in, uint8_t *data_out, int width, int height)
{
  int y, x;
  uint8_t *pPrev;
  uint8_t *p;
  uint8_t *pNext;
  uint8_t *dest = data_out;

  p = data_in;
  pNext = p + width;
  for (x=0; x < width; x++) {
    *dest = ((*p) * 2 + *pNext)/3;
    dest++;
    p++;
    pNext++;
  }

  pPrev = data_in;
  for (y=1; y < (height-1); y++) {
    for (x=0; x < width; x++) {
      *dest = (*pPrev + (*p) * 2 + *pNext)/4;
      dest++;
      pPrev++;
      p++;
      pNext++;
    }
  }

  for (x=0; x < width; x++) {
    *dest = (*pPrev + (*p) * 2)/3;
    dest++;
    pPrev++;
    p++;
  }
}


static void single_y3blend(uint8_t *data_in, uint8_t *data_out, int width, int height)
{
  /* 
   * I made this to smooth out the luma channel for videos captured
   * from the KWorld em28xx usb device.  It works pretty good, but
   * does blur things out a bit.  Running a sharpening filter
   * afterwards brings the noise right back.
   */
  int y, x;
  uint8_t *p;
  uint8_t *dest = data_out;

  p = data_in;
  for (y=0; y < height; y++) {
    *dest++ = *p++;
    for (x=1; x < width-1; x++) {
      uint16_t x = (*(p-1) + *p + *(p+1))/3; /* Average 3 adjacent values. */
      if (x > 255) { x = 255; }	/* Clamp. */
      *dest++ = x;
      p++;
    }
    *dest++ = *p++;
  }
}


static void adaptive3point_filter(YUVYUV422P_buffer *y422p_src, YUVYUV422P_buffer *y422p_out)
{
  /* Another attempt at cleaning up saturation artifacts. */
  int y, x;
  int i;
  
  static struct {
    int factors[3];
    int divisor;
  } filter[3] = {
    { {1, 1, 1}, 3},
    { {0, 1, 0}, 1},
    { {-2, 5, -2}, 3},
  };

  uint8_t *ysrc = y422p_src->y;
  uint8_t *cb = y422p_src->cb;
  uint8_t *cr = y422p_src->cr;
  uint8_t *yout = y422p_out->y;

  for (y=0; y < y422p_src->height; y++) {
    *yout++ = *ysrc++; 		/* copy first pixel */
    for (x=1; x < y422p_src->width - 1; x++) {
      if (0) {
	/* High saturation, smooth luma. */
	i = 0;
      }
      else if (0) {
	/* Low saturation, sharpen luma. */
	i = 2;
      }
      else {
	/* Medium saturation, keep luma. */
	i = 1;
      }
      int16_t k = (ysrc[-1]*filter[i].factors[0] 
		   + ysrc[0]*filter[i].factors[1] 
		   + ysrc[1]*filter[i].factors[2]) / filter[i].divisor;
      if (k > 255) { k = 255; } else if (k < 0) { k = 0; } /* Clamp. */
      *yout++ = k;
      ysrc++;

      if (x % 2 == 1) {
	/* Not sure if this is accurate regarding co-siting, but this
	   was written for post-processing messy analog video
	   anyway. */
	cr++;
	cb++;
      }
    }
    *yout++ = *ysrc++;		/* copy last pixel */
  }
  
  memcpy(y422p_out->cb, y422p_src->cb, y422p_out->cb_length);
  memcpy(y422p_out->cr, y422p_src->cr, y422p_out->cb_length);
}


static void single_horizontal_filter(VFilter_private *priv, uint8_t *data_in, uint8_t *data_out, int width, int height)
{
  int y, x;
  uint8_t *p;
  uint8_t *dest = data_out;

  p = data_in;
  for (y=0; y < height; y++) {
    *dest++ = *p++;
    *dest++ = *p++;
    for (x=2; x < width-2; x++) {
      int16_t x = 
	(*(p-2)*priv->horizontal_filter[0] +
	 *(p-1)*priv->horizontal_filter[1] +
	 *(p+0)*priv->horizontal_filter[2] +
	 *(p+1)*priv->horizontal_filter[3] +
	 *(p+1)*priv->horizontal_filter[4])
	/ priv->horizontal_filter_divisor;

      /* Clamp. */
      if (x > 255) { 
	x = 255; 
      }	
      else if (x < 0) {
	x = 0;
      }

      *dest++ = x;
      p++;
    }
    *dest++ = *p++;
    *dest++ = *p++;
  }
}


static void single_field_split(uint8_t *data, int width, int height)
{
  /* Take an interlaced image, push all the odd lines to the top, and even lines to the bottom. */
  int y;
  uint8_t *psrc;
  uint8_t *pdest;

  if (height % 2 == 1) {
    /* Only work on even number of rows. */
    height -= 1;
  }

  /* Copy of data plane, on stack. */
  uint8_t temp[width * height];
  memcpy(temp, data, sizeof(temp));
	 
  /* Copy first field.  Note that first line is already in place. */
  pdest = data + width;
  psrc = temp + (width * 2);
  for (y=2; y < height; y += 2) {
    memcpy(pdest, psrc, width);
    pdest += width;
    psrc += (width * 2);
  }

  /* Copy second field */
  pdest = data + (width * (height/2));
  psrc = temp + width;
  for (y=1; y < height; y += 2) {
    memcpy(pdest, psrc, width);
    pdest += width;
    psrc += (width * 2);
  }
}


static void single_trim(VFilter_private * priv, uint8_t *data_in, uint8_t *data_out, int width, int height)
{
  int i;
  uint8_t mask = (255 - (1 << priv->trim) - 1);
  for (i=0; i < (width*height); i++) {
    data_out[i] = data_in[i] & mask;
  }
}



static void Y422p_handler(Instance *pi, void *msg)
{
  VFilter_private *priv = (VFilter_private *)pi;
  YUVYUV422P_buffer *y422p_in = msg;
  YUVYUV422P_buffer *y422p_out = 0L;
  YUVYUV422P_buffer *y422p_src = 0L;

  /* FIXME: The crop operations could be done by calculations, followed by only a single copy operation. */
  if (priv->top_crop) {
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height - priv->top_crop, &y422p_src->c);
    memcpy(y422p_out->y, y422p_src->y+(y422p_src->width * priv->top_crop), y422p_out->width*y422p_out->height);
    memcpy(y422p_out->cb, y422p_src->cb+(y422p_src->width * priv->top_crop)/2, y422p_out->width*y422p_out->height/2);
    memcpy(y422p_out->cr, y422p_src->cr+(y422p_src->width * priv->top_crop)/2, y422p_out->width*y422p_out->height/2);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->bottom_crop) {
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height - priv->bottom_crop, &y422p_src->c);
    memcpy(y422p_out->y, y422p_src->y, y422p_out->width*y422p_out->height);
    memcpy(y422p_out->cb, y422p_src->cb, y422p_out->width*y422p_out->height/2);
    memcpy(y422p_out->cr, y422p_src->cr, y422p_out->width*y422p_out->height/2);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->left_right_crop) {
    y422p_src = y422p_out ? y422p_out : y422p_in;
    if (priv->left_right_crop > y422p_src->width) {
      fprintf(stderr, "left_right_crop value %d is wider than input %d\n", 
	      priv->left_right_crop, y422p_src->width);
    }
    else {
      y422p_out = YUVYUV422P_buffer_new(y422p_src->width - (priv->left_right_crop * 2), y422p_src->height, 
				   &y422p_src->c);
      memcpy(y422p_out->y, y422p_src->y+priv->left_right_crop, y422p_out->width);
      memcpy(y422p_out->cb, y422p_src->cb+(priv->left_right_crop/2), y422p_out->width/2);
      memcpy(y422p_out->cr, y422p_src->cr+(priv->left_right_crop/2), y422p_out->width/2);
      YUVYUV422P_buffer_discard(y422p_src);
    }
  }

  if (priv->y3blend) {
    /* Horizontal blend, for smoothing out saturation artifact in Y channel. */
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height, &y422p_src->c);
    single_y3blend(y422p_src->y, y422p_out->y, y422p_src->width, y422p_src->height);
    memcpy(y422p_out->cb, y422p_src->cb, y422p_src->width*y422p_src->height/2);
    memcpy(y422p_out->cr, y422p_src->cr, y422p_src->width*y422p_src->height/2);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->adaptive3point) {
    /* Another way to remote saturation artifacts. */
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height, &y422p_src->c);
    adaptive3point_filter(y422p_src, y422p_out);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->horizontal_filter_enabled) {
    /* Horizontal filter. */
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height, &y422p_src->c);
    single_horizontal_filter(priv, y422p_src->y, y422p_out->y, y422p_src->width, y422p_src->height);
    memcpy(y422p_out->cb, y422p_src->cb, y422p_src->width*y422p_src->height/2);
    memcpy(y422p_out->cr, y422p_src->cr, y422p_src->width*y422p_src->height/2);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->linear_blend) {
    /* Vertical blend, for cheap de-interlacing. */
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height, &y422p_src->c);
    single_121_linear_blend(y422p_src->y, y422p_out->y, y422p_src->width, y422p_src->height);
    single_121_linear_blend(y422p_src->cb, y422p_out->cb, y422p_src->width/2, y422p_src->height);
    single_121_linear_blend(y422p_src->cr, y422p_out->cr, y422p_src->width/2, y422p_src->height);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (priv->trim) {
    /* Smooth out low bits to make compression easier. */
    y422p_src = y422p_out ? y422p_out : y422p_in;
    y422p_out = YUVYUV422P_buffer_new(y422p_src->width, y422p_src->height, &y422p_src->c);
    single_trim(priv, y422p_src->y, y422p_out->y, y422p_src->width, y422p_src->height);
    YUVYUV422P_buffer_discard(y422p_src);
  }

  if (!y422p_out) {
    y422p_out = y422p_in; 
  }
  
  if (priv->field_split) {
    y422p_out->c.interlace_mode = IMAGE_FIELDSPLIT_TOP_FIRST;
    single_field_split(y422p_out->y, y422p_out->width, y422p_out->height);
    single_field_split(y422p_out->cb, y422p_out->width/2, y422p_out->height);
    single_field_split(y422p_out->cr, y422p_out->width/2, y422p_out->height);
  }
  
  if (pi->outputs[OUTPUT_YUV422P].destination) {
    PostData(y422p_out, pi->outputs[OUTPUT_YUV422P].destination);
  }
  else {
    YUVYUV422P_buffer_discard(y422p_out);
  }
  
}


static void RGB3_handler(Instance *pi, void *msg)
{
  VFilter_private *priv = (VFilter_private *)pi;
  RGB3_buffer *rgb3 = msg;  

  if (priv->trim) {
    single_trim(priv, rgb3->data, rgb3->data, rgb3->width*3, rgb3->height);
  }

  if (priv->top_crop) {
    RGB3_buffer *tmp = RGB3_buffer_new(rgb3->width, rgb3->height - priv->top_crop, &rgb3->c);
    memcpy(tmp->data, rgb3->data+(rgb3->width*priv->top_crop*3), tmp->data_length);
    RGB3_buffer_discard(rgb3);
    rgb3 = tmp;
  }

  if (priv->bottom_crop) {
    RGB3_buffer *tmp = RGB3_buffer_new(rgb3->width, rgb3->height - priv->bottom_crop, &rgb3->c);
    memcpy(tmp->data, rgb3->data, tmp->width * 3 * tmp->height);
    RGB3_buffer_discard(rgb3);
    rgb3 = tmp;
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
  }
  else {
    RGB3_buffer_discard(rgb3);
  }

  
}


static void VFilter_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void VFilter_instance_init(Instance *pi)
{
  // VFilter_private *priv = (VFilter_private *)pi;
}


static Template VFilter_template = {
  .label = "VFilter",
  .priv_size = sizeof(VFilter_private),
  .inputs = VFilter_inputs,
  .num_inputs = table_size(VFilter_inputs),
  .outputs = VFilter_outputs,
  .num_outputs = table_size(VFilter_outputs),
  .tick = VFilter_tick,
  .instance_init = VFilter_instance_init,
};

void VFilter_init(void)
{
  Template_register(&VFilter_template);
}
