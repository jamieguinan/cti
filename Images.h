#ifndef _IMAGES_H_
#define _IMAGES_H_

#include <stdint.h>
#include <sys/time.h>
#include "Mem.h"
#include "String.h"
#include "locks.h"

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
  IMAGE_TYPE_YUV420P,           /* decompressed iPhone Jpegs */
  IMAGE_TYPE_YUV422P,           /* raw dumps from V4L2Capture module, some decompressed Jpegs */
  IMAGE_TYPE_YUYV,		/* packed pixel 4:2:2 */
  IMAGE_TYPE_O511,
  IMAGE_TYPE_H264,              /* One unit of compressor output */
  AUDIO_TYPE_AAC,
} ImageType;

extern ImageType Image_guess_type(uint8_t * data, int len);

enum {
  IMAGE_INTERLACE_NONE,
  IMAGE_INTERLACE_TOP_FIRST,
  IMAGE_INTERLACE_BOTTOM_FIRST,
  IMAGE_FIELDSPLIT_TOP_FIRST,
  IMAGE_FIELDSPLIT_BOTTOM_FIRST,
  IMAGE_INTERLACE_MIXEDMODE,
};

/* Common metadata fields. */
typedef struct {
  double timestamp;
  uint32_t fps_numerator;
  uint32_t fps_denominator;
  double nominal_period;
  int interlace_mode;
  int eof;                      /* EOF marker. */
  LockedRef ref;
  String * label;               /* Optional source label. */
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


/* RGBA buffer. */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} RGBA_buffer;


/* BGR3 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} BGR3_buffer;


/* YUV422P buffer */
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
} YUV422P_buffer;

/* YUYV 422 packed pixel. */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} YUYV_buffer;


/* 420P buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;   /* "data" contains all components; y,cb,cr point into this */
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
} YUV420P_buffer;


/* Jpeg buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  unsigned long data_length;      /* Provisional. */
  unsigned long encoded_length;   /* Actual encoded jpeg size. */
  int has_huffman_tables;         /* Set by Jpeg_fix(). */

  Image_common c;
} Jpeg_buffer;


/* O511 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  unsigned long data_length;            /* Provisional. */
  unsigned long encoded_length;         /* Actual encoded size. */

  Image_common c;
} O511_buffer;


/* H264 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;                /* NAL unit data (may include several) */
  int data_length;              /* Provisional. */
  int encoded_length;           /* Actual encoded size. */

  int keyframe;                 /* IDR keyframe present */

  Image_common c;
} H264_buffer;

extern Gray_buffer *Gray_buffer_new(int width, int height, Image_common *c);
extern Gray_buffer *PGM_buffer_from(uint8_t *data, int len, Image_common *c);
extern Gray_buffer *Gray_buffer_from(uint8_t *data, int width, int height, Image_common *c);
extern void Gray_buffer_release(Gray_buffer *gray);

extern Gray32_buffer *Gray32_buffer_new(int width, int height, Image_common *c);
extern void Gray32_buffer_release(Gray32_buffer *gray);

extern RGB3_buffer *PPM_buffer_from(uint8_t *data, int len, Image_common *c);
extern RGB3_buffer *RGB3_buffer_new(int width, int height, Image_common *c);
extern RGB3_buffer *RGB3_buffer_clone(RGB3_buffer *rgb);
extern RGB3_buffer *RGB3_buffer_ref(RGB3_buffer *rgb);
extern void RGB3_buffer_release(RGB3_buffer *rgb);
extern void RGB_buffer_merge_rgba(RGB3_buffer *rgb, uint8_t *rgba, int width, int height, int stride);

extern RGBA_buffer *RGBA_buffer_new(int width, int height, Image_common *c);
extern RGBA_buffer *RGBA_buffer_clone(RGBA_buffer *rgb);
extern RGBA_buffer *RGBA_buffer_ref(RGBA_buffer *rgb);
extern void RGBA_buffer_release(RGBA_buffer *rgb);

extern BGR3_buffer *BGR3_buffer_new(int width, int height, Image_common *c);
extern void BGR3_buffer_release(BGR3_buffer *rgb);

/* In-place conversion. */
void bgr3_to_rgb3(BGR3_buffer **bgr, RGB3_buffer **rgb);
void rgb3_to_bgr3(RGB3_buffer **rgb, BGR3_buffer **bgr);


extern YUV422P_buffer *YUV422P_buffer_new(int width, int height, Image_common *c);
extern YUV422P_buffer *YUV422P_buffer_ref(YUV422P_buffer *y422p);
extern YUV422P_buffer *YUV422P_buffer_from(uint8_t *data, int len, Image_common *c);
extern void YUV422P_buffer_release(YUV422P_buffer *y422p);
extern YUV422P_buffer *YUV422P_copy(YUV422P_buffer *y422p, int x, int y, int width, int height);
extern YUV422P_buffer *YUV422P_clone(YUV422P_buffer *y422p);
extern void YUV422P_paste(YUV422P_buffer *dest, YUV422P_buffer *src, int x, int y, int width, int height);
extern RGB3_buffer *YUV422P_to_RGB3(YUV422P_buffer *y422p);
extern BGR3_buffer *YUV422P_to_BGR3(YUV422P_buffer *y422p);
extern Gray_buffer *YUV422P_to_Gray(YUV422P_buffer *yuv422p);

extern YUV420P_buffer *YUV420P_buffer_new(int width, int height, Image_common *c);
extern YUV420P_buffer *YUV420P_buffer_ref(YUV420P_buffer *y420p);
extern YUV420P_buffer *YUV420P_buffer_from(uint8_t *data, int width, int height, Image_common *c);
extern YUV420P_buffer *YUV420P_clone(YUV420P_buffer *y420p);
extern void YUV420P_buffer_release(YUV420P_buffer *y420p);
extern RGB3_buffer *YUV420P_to_RGB3(YUV420P_buffer *y420p);
extern BGR3_buffer *YUV420P_to_BGR3(YUV420P_buffer *y420p);
extern Gray_buffer *YUV420P_to_Gray(YUV420P_buffer *yuv420p);
extern YUV420P_buffer *YUV422P_to_YUV420P(YUV422P_buffer *y422p);
extern YUV422P_buffer *YUV420P_to_YUV422P(YUV420P_buffer *yuv420p);

extern YUYV_buffer *YUYV_buffer_new(int width, int height, Image_common *c);
extern void YUYV_buffer_release(YUYV_buffer *yuyv);

/* extra conversions between non-compressed formats */
extern YUV422P_buffer *RGB3_to_YUV422P(RGB3_buffer *rgb);
extern YUV422P_buffer *BGR3_toYUV422P(BGR3_buffer *bgr);
extern YUV420P_buffer *RGB3_to_YUV420P(RGB3_buffer *rgb);

extern Jpeg_buffer *Jpeg_buffer_new(int size, Image_common *c);
extern Jpeg_buffer *Jpeg_buffer_from(uint8_t *data, unsigned long data_length, Image_common *c);
extern void Jpeg_buffer_release(Jpeg_buffer *jpeg);
extern Jpeg_buffer *Jpeg_buffer_ref(Jpeg_buffer *jpeg);
extern void Jpeg_fix(Jpeg_buffer *jpeg);
extern RGB3_buffer * Jpeg_to_rgb3(Jpeg_buffer * jpeg);

extern O511_buffer *O511_buffer_new(int width, int height, Image_common *c);
extern O511_buffer *O511_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c);
extern void O511_buffer_release(O511_buffer *o511);

extern H264_buffer *H264_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c);
extern void H264_buffer_release(H264_buffer *h264);



#endif
