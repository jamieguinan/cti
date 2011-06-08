#ifndef _IMAGES_H_
#define _IMAGES_H_

#include <stdint.h>
#include <sys/time.h>


/* http://en.wikipedia.org/wiki/Layers_%28digital_image_editing%29 */
typedef struct {
  /* I might not use this, but instead use specific buffer types.  That will avoid
     lots of if/else switching.  C++ might have helped there, I admit, but I'm doing
     this project in C. */
} Layer;


/* Gray buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;
  struct timeval tv;
  float nominal_period;
} Gray_buffer;


/* RGB3 buffer. */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;
  struct timeval tv;
  float nominal_period;
} RGB3_buffer;

/* BGR3 buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;
  struct timeval tv;
  float nominal_period;
} BGR3_buffer;


/* 422P buffer */
typedef struct {
  int width;
  int height;
  uint8_t *y;
  int y_length;
  uint8_t *cb;
  int cb_length;
  uint8_t *cr;
  int cr_length;
  struct timeval tv;
  float nominal_period;
} Y422P_buffer;

/* 420P buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  uint8_t *y;
  int y_length;
  uint8_t *cb;
  int cb_length;
  uint8_t *cr;
  int cr_length;
  struct timeval tv;
  float nominal_period;
} Y420P_buffer;

/* Jpeg buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded jpeg size. */
  struct timeval tv;
  float nominal_period;
} Jpeg_buffer;

/* O511 buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */
  struct timeval tv;
  float nominal_period;
} O511_buffer;

/* H264 buffer */
typedef struct {
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */
  struct timeval tv;
  float nominal_period;
} H264_buffer;


extern Gray_buffer *Gray_buffer_new(int width, int height);
extern void Gray_buffer_discard(Gray_buffer *gray);

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
