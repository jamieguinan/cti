#ifndef _JPEG_MISC_H_
#define _JPEG_MISC_H_

#include "Images.h"

typedef struct {
  const char *label;
  ImageType imgtype;
  int libjpeg_colorspace;
  const char * libjpeg_colorspace_label;
  int factors[6];
  int crcb_width_divisor;
  int crcb_height_divisor;
} FormatInfo;

extern void Jpeg_decompress(Jpeg_buffer * jpeg_in,
                            int dct_method,
                            YUV420P_buffer ** yuv420p_result,
                            YUV422P_buffer ** yuv422p_result,
                            FormatInfo ** pfmt);


#endif
