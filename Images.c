#include <stdio.h>
#include <string.h>
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"

#define _min(a, b)  ((a) < (b) ? (a) : (b))

Gray_buffer *Gray_buffer_new(int width, int height)
{
  Gray_buffer *gray = Mem_malloc(sizeof(Gray_buffer));
  gray->width = width;
  gray->height = height;
  gray->data_length = width * height;
  gray->data = Mem_calloc(1, gray->data_length); 	/* Caller must fill in data! */
  return gray;
}

void Gray_buffer_discard(Gray_buffer *gray)
{
  Mem_free(gray->data);
  memset(gray, 0, sizeof(*gray));
  Mem_free(gray);
}


RGB3_buffer *RGB3_buffer_new(int width, int height)
{
  RGB3_buffer *rgb = Mem_malloc(sizeof(*rgb));
  rgb->width = width;
  rgb->height = height;
  rgb->data_length = width * height * 3;
  rgb->data = Mem_calloc(1, rgb->data_length); 	/* Caller must fill in data! */
  return rgb;
}

void RGB_buffer_merge_rgba(RGB3_buffer *rgb, uint8_t *rgba, int width, int height, int stride)
{
  int x=0;
  int y;
  int x_in = 0;
  int y_in;
  int offsetx = 0;
  int offsety = 0;
  int copied = 0;

  // printf("stride=%d\n", stride);

  for (y = offsety, y_in = 0; y >= 0 && y < rgb->height && y < (offsety+height) && y_in < height; y++, y_in++) {
    for (x = offsetx, x_in = 0; x >= 0 && x < rgb->width && x < (offsetx+width) && x_in < width; x++, x_in++) {
      uint8_t *prgb = rgb->data + (((y*rgb->width) + x) * 3);
      uint8_t *prgba = rgba + y_in*stride + x_in*4;
      uint8_t r, g, b, a;

      /* Cariro uses native-endian packed 32-bit values */
      b = prgba[0];
      g = prgba[1];
      r = prgba[2];
      a = prgba[3];

      /* RGB3_buffer uses PPM-style interleaved bytes.   Copy data using alpha channel. */
      prgb[0] = (((255 - a) * prgb[0]) + (a * r))/255;
      prgb[1] = (((255 - a) * prgb[1]) + (a * g))/255;
      prgb[2] = (((255 - a) * prgb[2]) + (a * b))/255;

      copied += 1;
    }
  }

  if (0) { 
    FILE *f = fopen("/dev/shm/rgb.ppm", "wb");
    FILE *g = fopen("/dev/shm/a.pgm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fprintf(g, "P5\n%d %d\n255\n", width, height);
    int y, x;
    uint8_t *p = rgba;
    for (y=0; y < height; y++) {
      for (x=0; x < width; x++) {
	uint8_t data[3];
	data[0] = p[0];
	data[1] = p[1];
	data[2] = p[2];
	if (fwrite(data, 3, 1, f) != 1) { perror("fwrite"); }
	if (fwrite(p+3, 1, 1, g) != 1) { perror("fwrite"); }
	p += 4;
      }
    }
    fclose(g);
    fclose(f);
  }

  if (cfg.verbosity) {
    printf("%d pixels copied (%d,%d) to (%d,%d)\n", 
	   copied, 
	   offsetx, offsety,
	   x, y);
  }
}

void RGB3_buffer_discard(RGB3_buffer *rgb)
{
  Mem_free(rgb->data);
  memset(rgb, 0, sizeof(*rgb));
  Mem_free(rgb);
}


BGR3_buffer *BGR3_buffer_new(int width, int height)
{
  BGR3_buffer *rgb = Mem_malloc(sizeof(BGR3_buffer));
  rgb->width = width;
  rgb->height = height;
  rgb->data_length = width * height * 3;
  rgb->data = Mem_calloc(1, rgb->data_length); 	/* Caller must fill in data! */
  return rgb;
}

void BGR3_buffer_discard(BGR3_buffer *rgb)
{
  Mem_free(rgb->data);
  memset(rgb, 0, sizeof(*rgb));
  Mem_free(rgb);
}


static void swap_0_2(uint8_t *buffer, int length)
{
  int i;
  for (i=0; i < length; i+=3) {
    uint8_t tmp;
    tmp = buffer[i];
    buffer[i] = buffer[i+2];
    buffer[i+2] = tmp;
  }
}


/* Dirty hack... */
void bgr3_to_rgb3(BGR3_buffer **bgr, RGB3_buffer **rgb)
{
  *rgb = (RGB3_buffer*)*bgr;
  *bgr = 0L;
  swap_0_2((*rgb)->data, (*rgb)->data_length);
}

void rgb3_to_bgr3(RGB3_buffer **rgb, BGR3_buffer **bgr)
{
  *bgr = (BGR3_buffer*)*rgb;
  *rgb = 0L;
  swap_0_2((*bgr)->data, (*bgr)->data_length);
}



Y422P_buffer * Y422P_buffer_new(int width, int height)
{
  Y422P_buffer * y422p = Mem_malloc(sizeof(*y422p));
  y422p->width = width;
  y422p->height = height;
  y422p->y_length = width*height;
  y422p->cr_length = width*height/2;
  y422p->cb_length = width*height/2;
  y422p->y = Mem_calloc(1, y422p->y_length+1); y422p->y[y422p->y_length] = 0x55;
  y422p->cr = Mem_calloc(1, y422p->cr_length+1);  y422p->cr[y422p->cr_length] = 0x55;
  y422p->cb = Mem_calloc(1, y422p->cb_length+1);  y422p->cb[y422p->cb_length] = 0x55;
  
  return y422p;
}

void Y422P_buffer_discard(Y422P_buffer *y422p)
{
  if (y422p->cb[y422p->cb_length] != 0x55) { fprintf(stderr, "cb buffer spilled!\n"); }
  Mem_free(y422p->cb);
  if (y422p->cr[y422p->cr_length] != 0x55) { fprintf(stderr, "cr buffer spilled!\n"); }
  Mem_free(y422p->cr);
  if (y422p->y[y422p->y_length] != 0x55) { fprintf(stderr, "y buffer spilled!\n"); }
  Mem_free(y422p->y);
  memset(y422p, 0, sizeof(*y422p));
  Mem_free(y422p);
}

static void Y422P_to_xGx(Y422P_buffer *y422p, RGB3_buffer *rgb, BGR3_buffer *bgr)
{
  // R = Y + 1.402 (Cr-128)
  // G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)
  // B = Y + 1.772 (Cb-128)
  
  int x, y;
  uint8_t *prgb = 0L;
  uint8_t *pbgr = 0L;

  if (rgb) {
    prgb = rgb->data;
  }

  if (bgr) {
    pbgr = bgr->data;
  }

  uint8_t *pY = y422p->y;
  uint8_t *pCr = y422p->cr;
  uint8_t *pCb = y422p->cb;

  for (y = 0; y < y422p->height; y++) {
    int R, G, B, Y, Cr = 0, Cb = 0;
    for (x = 0; x < y422p->width; x+=1) {
      if (x % 2 == 0) {
	pCr++;
	pCb++;
	Cr = *pCr;
	Cb = *pCb;
      }

      Y = *pY;
      R = Y + 1.402 * (Cr - 128);
      G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128);
      B = Y + 1.772 * (Cb - 128);

      if (R > 255) { R = 255; } else if (R < 0) { R = 0; } 
      if (G > 255) { G = 255; } else if (G < 0) { G = 0; } 
      if (B > 255) { B = 255; } else if (B < 0) { B = 0; } 

      if (prgb) {
	prgb[0] = R;
	prgb[1] = G;
	prgb[2] = B;
	prgb += 3;
      }

      if (pbgr) {
	pbgr[0] = B;
	pbgr[1] = G;
	pbgr[2] = R;
	pbgr += 3;
      }
      pY++;
    }
  }
}


RGB3_buffer *Y422P_to_RGB3(Y422P_buffer *y422p)
{
  RGB3_buffer *rgb = RGB3_buffer_new(y422p->width, y422p->height);
  Y422P_to_xGx(y422p, rgb, 0L);
  return rgb;
}


BGR3_buffer *Y422P_to_BGR3(Y422P_buffer *y422p)
{
  BGR3_buffer *bgr = BGR3_buffer_new(y422p->width, y422p->height);
  Y422P_to_xGx(y422p, 0L, bgr);
  return bgr;
}


Y422P_buffer *RGB3_toY422P(RGB3_buffer *rgb)
{
  //Y = 0.299R + 0.587G + 0.114B
  //U'= (B-Y)*0.565
  //V'= (R-Y)*0.713

  int x, y;
  Y422P_buffer *y422p = Y422P_buffer_new(rgb->width, rgb->height);

  uint8_t *p = rgb->data;
  uint8_t *pY = y422p->y;
  uint8_t *pCr = y422p->cr;
  uint8_t *pCb = y422p->cb;

  for (y=0; y < rgb->height; y++) {
    for (x = 0; x < rgb->width; x+=1) {
      uint8_t R = p[0];
      uint8_t G = p[1];
      uint8_t B = p[2];
      uint8_t Y;

      Y = _min(0.299*R + 0.587*G + 0.114*B, 255);
      *pY = Y;

      if (x % 2 == 0) {
	*pCb = _min((B - Y)*0.565 + 128, 255);
	*pCr = _min((R - Y)*0.713 + 128, 255);
	pCr++;
	pCb++;
      }

      p += 3;
      pY++;
    }

  }

  return y422p;
}


Y422P_buffer *BGR3_toY422P(BGR3_buffer *bgr)
{
  //Y = 0.299R + 0.587G + 0.114B
  //U'= (B-Y)*0.565
  //V'= (R-Y)*0.713

  int x, y;
  Y422P_buffer *y422p = Y422P_buffer_new(bgr->width, bgr->height);

  uint8_t *p = bgr->data;
  uint8_t *pY = y422p->y;
  uint8_t *pCr = y422p->cr;
  uint8_t *pCb = y422p->cb;

  for (y=0; y < bgr->height; y++) {
    for (x = 0; x < bgr->width; x+=1) {
      uint8_t B = p[0];
      uint8_t G = p[1];
      uint8_t R = p[2];
      uint8_t Y;

      Y = _min(0.299*R + 0.587*G + 0.114*B, 255);
      *pY = Y;

      if (x % 2 == 0) {
	*pCb = _min((B - Y)*0.565 + 128, 255);
	*pCr = _min((R - Y)*0.713 + 128, 255);
	pCr++;
	pCb++;
      }

      p += 3;
      pY++;
    }

  }

  return y422p;
}


Y420P_buffer * Y420P_buffer_new(int width, int height)
{
  Y420P_buffer * y420p = Mem_malloc(sizeof(*y420p));
  y420p->width = width;
  y420p->height = height;
  y420p->y_length = width*height;
  y420p->cr_length = width*height/4;
  y420p->cb_length = width*height/4;

  /* One data allocation, component pointers are set within it. */
  y420p->data = Mem_calloc(1, y420p->y_length+y420p->cr_length+y420p->cb_length);

  y420p->y = y420p->data;
  y420p->cr = y420p->data + y420p->y_length;
  y420p->cb = y420p->data + y420p->y_length + y420p->cr_length;
  
  return y420p;
}

void Y420P_buffer_discard(Y420P_buffer *y420p)
{
  Mem_free(y420p->data);
  memset(y420p, 0, sizeof(*y420p));
  Mem_free(y420p);
}

Y420P_buffer *Y422P_to_Y420P(Y422P_buffer *y422p)
{
  Y420P_buffer * y420p = Y420P_buffer_new(y422p->width, y422p->height);
  int x, y;
  int n = 0;

  memcpy(y420p->y, y422p->y, y420p->y_length);

  for (y = 0; y < y422p->height; y+=2) {
    for (x = 0; x < y422p->width/2; x+=1) {
      y420p->cb[n] = (y422p->cb[y*y422p->width/2 + x] + y422p->cb[(y+1)*y422p->width/2 + x])/2;
      y420p->cr[n] = (y422p->cr[y*y422p->width/2 + x] + y422p->cr[(y+1)*y422p->width/2 + x])/2;
      n++;
    }
  }

  // printf("n=%d\n", n);
  return y420p;
}


RGB3_buffer *Y420P_to_RGB3(Y420P_buffer *y420p)
{
  RGB3_buffer *rgb = RGB3_buffer_new(y420p->width, y420p->height);

  int x, y;
  int r, g, b;
  int c, d, e;

  int w = y420p->width;
  int h = y420p->height;
  
  uint8_t *yuvin = y420p->data;
  uint8_t *rgbout8 = rgb->data;

  uint8_t *pY;
  uint8_t *pU;
  uint8_t *pV;

  for (y=0; y < h; y+=1) {
    pY = yuvin + (w * y);
    pU = yuvin + (w * h) + (y/2)*(w/2);
    pV = yuvin + (w*h*5/4) + (y/2)*(w/2);

    for (x=0; x < w; x++) {

      c = *pY - 16;
      d = *pU - 128;
      e = *pV - 128;

#define clamp(x) (x < 0 ? 0 : (x > 255 ? 255 : x))

      r = clamp(( 298 * c +   0 * d + 409 * e + 128) >> 8);
      g = clamp(( 298 * c - 100 * d - 208 * e + 128) >> 8);
      b = clamp(( 298 * c + 516 * d +   0 * e + 128) >> 8);

      *rgbout8++ = r;
      *rgbout8++ = g;
      *rgbout8++ = b;

      pY += 1;
      if ((long)pY & 1) {
	pU++;
	pV++;
      }
    }
  }
  
  return rgb;
}


Jpeg_buffer *Jpeg_buffer_new(int size)
{
  Jpeg_buffer *jpeg = Mem_malloc(sizeof(Jpeg_buffer));
  jpeg->width = -1;
  jpeg->height = -1;
  jpeg->data_length = size;
  jpeg->data = Mem_calloc(1, jpeg->data_length);	/* Caller must fill in data! */
  return jpeg;
}

void Jpeg_buffer_discard(Jpeg_buffer *jpeg)
{
  Mem_free(jpeg->data);
  memset(jpeg, 0, sizeof(*jpeg));
  Mem_free(jpeg);
}


O511_buffer *O511_buffer_new(int width, int height)
{
  O511_buffer *o511 = Mem_malloc(sizeof(O511_buffer));
  o511->width = width;
  o511->height = height;
  o511->data_length = (width*height*3)/2;
  o511->data = Mem_calloc(1, o511->data_length);

  return o511;
}

O511_buffer *O511_buffer_from(uint8_t *data, int data_length, int width, int height)
{
  O511_buffer *o511 = Mem_malloc(sizeof(O511_buffer));
  o511->width = width;
  o511->height = height;
  o511->data_length = data_length;
  o511->data = Mem_calloc(1, o511->data_length);
  memcpy(o511->data, data, data_length);
  o511->encoded_length = o511->data_length; /* same as data_length */

  return o511;
}

void O511_buffer_discard(O511_buffer *o511)
{
  Mem_free(o511->data);
  memset(o511, 0, sizeof(*o511));
  Mem_free(o511);
}


H264_buffer *H264_buffer_from(uint8_t *data, int data_length, int width, int height)
{
  H264_buffer *h264 = Mem_malloc(sizeof(H264_buffer));
  h264->width = width;
  h264->height = height;
  h264->data_length = data_length;
  h264->data = Mem_calloc(1, h264->data_length);
  memcpy(h264->data, data, data_length);
  h264->encoded_length = h264->data_length; /* same as data_length */

  return h264;
}


void H264_buffer_discard(H264_buffer *h264)
{
  Mem_free(h264->data);
  memset(h264, 0, sizeof(*h264));
  Mem_free(h264);
}
