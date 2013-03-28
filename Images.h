#ifndef _IMAGES_H_
#define _IMAGES_H_

#include <stdint.h>
#include <sys/time.h>
#include "Mem.h"

/* http://en.wikipedia.org/wiki/Layers_%28digital_image_editing%29 */
typedef struct {
  /* I might not use this, but instead use specific buffer types.  That will avoid
     lots of if/else switching.  C++ might have helped there, I admit, but I'm doing
     this project in C. */
} Layer;


/* Enumerate image types, CTI internal use only. */
typedef enum {
  IMAGE_TYPE_UNKNOWN,
  IMAGE_TYPE_JPEG,
  IMAGE_TYPE_PGM,
  IMAGE_TYPE_PPM,
} ImageType;

extern ImageType Image_guess_type(uint8_t * data, int len);

enum { 
  IMAGE_INTERLACE_NONE,
  IMAGE_INTERLACE_TOP_FIRST,
  IMAGE_INTERLACE_BOTTOM_FIRST,
  IMAGE_FIELDSPLIT_TOP_FIRST,
  IMAGE_FIELDSPLIT_BOTTOM_FIRST,
};

/* Common metadata fields. */
typedef struct {
  struct timeval tv;
  float nominal_period;
  int interlace_mode;
} Image_common;


/* Gray buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} Gray_buffer;


/* Gray 32-bit buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint32_t *data;
  int data_length;
  Image_common c;
} Gray32_buffer;


/* RGB3 buffer. */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} RGB3_buffer;

/* BGR3 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} BGR3_buffer;


/* 422P buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *y;
  int y_length;

  uint8_t *cb;
  int cb_width;
  int cb_height;
  int cb_length;

  uint8_t *cr;
  int cr_width;
  int cr_height;
  int cr_length;

  Image_common c;
} Y422P_buffer;

/* 420P buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  uint8_t *y;
  int y_length;
  uint8_t *cb;
  int cb_length;
  uint8_t *cr;
  int cr_length;

  Image_common c;
} Y420P_buffer;

/* Jpeg buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded jpeg size. */

  Image_common c;
} Jpeg_buffer;

/* O511 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */

  Image_common c;
} O511_buffer;

/* H264 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */

  Image_common c;
} H264_buffer;


extern Gray_buffer *Gray_buffer_new(int width, int height);
extern Gray_buffer *PGM_buffer_from(uint8_t *data, int len);
extern void Gray_buffer_discard(Gray_buffer *gray);

extern Gray32_buffer *Gray32_buffer_new(int width, int height);
extern void Gray32_buffer_discard(Gray32_buffer *gray);
extern RGB3_buffer *PPM_buffer_from(uint8_t *data, int len);
extern RGB3_buffer *RGB3_buffer_new(int width, int height);

extern void RGB3_buffer_discard(RGB3_buffer *rgb);
extern void RGB_buffer_merge_rgba(RGB3_buffer *rgb, uint8_t *rgba, int width, int height, int stride);

extern BGR3_buffer *BGR3_buffer_new(int width, int height);
extern void BGR3_buffer_discard(BGR3_buffer *rgb);

/* In-place conversion. */
void bgr3_to_rgb3(BGR3_buffer **bgr, RGB3_buffer **rgb);
void rgb3_to_bgr3(RGB3_buffer **rgb, BGR3_buffer **bgr);


extern Y422P_buffer *Y422P_buffer_new(int width, int height);
extern void Y422P_buffer_discard(Y422P_buffer *y422p);
extern Y422P_buffer *Y422P_copy(Y422P_buffer *y422p, int x, int y, int width, int height);
extern Y422P_buffer *Y422P_clone(Y422P_buffer *y422p);
extern void Y422P_paste(Y422P_buffer *dest, Y422P_buffer *src, int x, int y, int width, int height);
extern RGB3_buffer *Y422P_to_RGB3(Y422P_buffer *y422p);
extern BGR3_buffer *Y422P_to_BGR3(Y422P_buffer *y422p);

extern Y420P_buffer *Y420P_buffer_new(int width, int height);
extern void Y420P_buffer_discard(Y420P_buffer *y420p);
extern RGB3_buffer *Y420P_to_RGB3(Y420P_buffer *y420p);

extern Y420P_buffer *Y422P_to_Y420P(Y422P_buffer *y422p);

extern Y422P_buffer *RGB3_toY422P(RGB3_buffer *rgb);
extern Y422P_buffer *BGR3_toY422P(BGR3_buffer *bgr);

extern Jpeg_buffer *Jpeg_buffer_new(int size);
extern Jpeg_buffer *Jpeg_buffer_from(uint8_t *data, int data_length); /* DJpeg.c */
extern void Jpeg_buffer_discard(Jpeg_buffer *jpeg);

extern O511_buffer *O511_buffer_new(int width, int height);
extern O511_buffer *O511_buffer_from(uint8_t *data, int data_length, int width, int height);
extern void O511_buffer_discard(O511_buffer *o511);

extern H264_buffer *H264_buffer_from(uint8_t *data, int data_length, int width, int height);
extern void H264_buffer_discard(H264_buffer *h264);

#endif
