#include <stdio.h>		/* FILE for jpeglib.h */
#include <setjmp.h>		/* setjmp */
#include <stdlib.h>		/* exit */

#include "Images.h"
#include "Mem.h"
#include "ArrayU8.h"
#include "jpeglib.h"

#include "cdjpeg.h"
#include "wrmem.h"
#include "jmemsrc.h"
#include "jpeghufftables.h"
#include "jpeg_misc.h"

#ifndef cti_table_size
#define cti_table_size(x) (sizeof(x)/sizeof(x[0]))
#endif

static void save_error_jpeg(uint8_t *data, int data_length)
{
  char filename[256];
  sprintf(filename, "error-%ld.jpg", time(NULL));
  FILE *f = fopen(filename, "wb");
  int n = fwrite(data, 1, data_length, f);
  if (n != data_length) {
    perror("fwrite"); 
  } else {
    printf("saved erroneous jpeg to error.jpg\n");
    fclose(f);
  }
}


static void jerr_error_handler(j_common_ptr cinfo)
{
  (*cinfo->err->output_message) (cinfo);
  if (cinfo->client_data) {
    jmp_buf *jb = cinfo->client_data;
    fprintf(stderr, "(recovering)\n");
    longjmp(*jb, 1);
  }
  else {
    exit(1);
  }
}

static FormatInfo known_formats[] = {
  /* See swdev/notes.txt regarding subsampling and formats. */
  { .imgtype = IMAGE_TYPE_YUV420P, .label = "YUV420P", 
    .libjpeg_colorspace = JCS_YCbCr, .libjpeg_colorspace_label = "JCS_YCbCr",
    .factors = { 2, 2, 1, 1, 1, 1}, .crcb_width_divisor = 2, .crcb_height_divisor = 2},

  { .imgtype = IMAGE_TYPE_YUV422P, .label = "YUV422P", 
    .libjpeg_colorspace = JCS_YCbCr, .libjpeg_colorspace_label = "JCS_YCbCr",
    .factors = { 2, 1, 1, 1, 1, 1}, .crcb_width_divisor = 2, .crcb_height_divisor = 1},

  { .imgtype = IMAGE_TYPE_YUV422P, .label = "YUV422P", 
    .libjpeg_colorspace = JCS_YCbCr, .libjpeg_colorspace_label = "JCS_YCbCr",
    .factors = { 2, 2, 1, 2, 1, 2}, .crcb_width_divisor = 2, .crcb_height_divisor = 1},
};


/* Create a Jpeg buffer from raw jpeg data.  This can return NULL if
   invalid jpeg data!  I put this function in this file rather than
   Images.c because this module includes jpeglib.h, and if I'm
   building on a system without the jpeg library, I still want
   Images.c to build, without requiring a bunch of #ifdefs */
Jpeg_buffer *Jpeg_buffer_from(uint8_t *data, int data_length, Image_common *c)
{
  Jpeg_buffer *jpeg = 0L;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jmp_buf jb;
  int error = 0;

  if (Image_guess_type(data, 5) != IMAGE_TYPE_JPEG) {
    fprintf(stderr, "%s: data does not look like a jpeg file [%02x %02x %02x %02x]\n", 
	    __func__, data[0], data[1], data[2], data[3]);
    return NULL;    
  }

  cinfo.err = jpeg_std_error(&jerr); /* NOTE: See ERREXIT, error_exit, 
					this may cause the program to call 
					exit()! */
  jerr.error_exit = jerr_error_handler;

  jpeg_create_decompress(&cinfo);

  cinfo.client_data = &jb;
  error = setjmp(jb);
  if (error) {
    fprintf(stderr, "%s:%d\n", __func__, __LINE__);
    if (0) {
      save_error_jpeg(data, data_length);
    }
    goto out;
  }

  jpeg_mem_src(&cinfo, data, data_length);

  (void) jpeg_read_header(&cinfo, TRUE); /* Possible path to setjmp() above. */

  // printf("jpeg: width=%d height=%d\n", cinfo.image_width, cinfo.image_height);

  /* If no error, allocate and return jpeg, otherwise it will return NULL. */
  jpeg = Jpeg_buffer_new(data_length, c);
  jpeg->width = cinfo.image_width;
  jpeg->height = cinfo.image_height;
  jpeg->data_length = data_length;
  jpeg->encoded_length = data_length;
  memcpy(jpeg->data, data, data_length);

 out:  
  jpeg_destroy_decompress(&cinfo); 
  
  return jpeg;
}

static void jerr_warning_noop(j_common_ptr cinfo, int msg_level)
{
}


void Jpeg_decompress(Jpeg_buffer * jpeg_in, 
		     YUV420P_buffer ** yuv420p_result,
		     YUV422P_buffer ** yuv422p_result,
		     FormatInfo ** pfmt)
{
  //int save_width = 0;
  //int save_height = 0;
  int i;

  YUV420P_buffer * yuv420p = NULL;
  YUV422P_buffer * yuv422p = NULL;

  /* Decompress to YCrCb, then convert to outputs as needed. */
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  int error = 0;
  jmp_buf jb;
  
  cinfo.err = jpeg_std_error(&jerr); /* NOTE: See ERREXIT, error_exit, 
					this may cause the program to call exit()! */
  jerr.emit_message = jerr_warning_noop;
  jerr.error_exit = jerr_error_handler;    
  
  jpeg_create_decompress(&cinfo);
  
  cinfo.client_data = &jb;
  error = setjmp(jb);
  if (error) {
    fprintf(stderr, "%s:%d\n", __func__, __LINE__);
    goto jdd;
  }
  
  cinfo.raw_data_out = TRUE;

  jpeg_mem_src(&cinfo, jpeg_in->data, jpeg_in->data_length);

  (void) jpeg_read_header(&cinfo, TRUE);

  //save_width = cinfo.image_width;
  //save_height = cinfo.image_height;
  
  int samp_factors[6] = {
    cinfo.comp_info[0].h_samp_factor,
    cinfo.comp_info[0].v_samp_factor,
    cinfo.comp_info[1].h_samp_factor,
    cinfo.comp_info[1].v_samp_factor,
    cinfo.comp_info[2].h_samp_factor,
    cinfo.comp_info[2].v_samp_factor,
  };

  FormatInfo * fmt = NULL;

  for (i=0; i < cti_table_size(known_formats); i++) {
    if (memcmp(samp_factors, known_formats[i].factors, sizeof(samp_factors)) == 0) {
      // dpf("Jpeg subsampling: %s\n", known_formats[i].label);
      fmt = &(known_formats[i]);
      *pfmt = fmt;
      break;
    }
  }
    
  if (!fmt) {
    printf("jpeg colorspace is %s\n",
	   cinfo.jpeg_color_space == JCS_GRAYSCALE ? "JCS_GRAYSCALE" :
	   cinfo.jpeg_color_space == JCS_RGB ? "JCS_RGB" :
	   cinfo.jpeg_color_space == JCS_YCbCr ? "JCS_YCbCr" :
	   "unknown");
    printf("%s: unhandled colorspace and/or subsampling: { %d, %d, %d, %d, %d, %d }\n", __func__,
	   samp_factors[0],
	   samp_factors[1],
	   samp_factors[2],
	   samp_factors[3],
	   samp_factors[4],
	   samp_factors[5]);
    goto out;
  }


  if (0) {
    fprintf(stderr, "cinfo.comp_info[0].h_samp_factor=%d\n", cinfo.comp_info[0].h_samp_factor);
    fprintf(stderr, "cinfo.comp_info[0].v_samp_factor=%d\n", cinfo.comp_info[0].v_samp_factor);
    fprintf(stderr, "cinfo.comp_info[0].downsampled_width=%d\n", cinfo.comp_info[0].downsampled_width);
    fprintf(stderr, "cinfo.comp_info[0].downsampled_height=%d\n", cinfo.comp_info[0].downsampled_height);
    fprintf(stderr, "cinfo.comp_info[1].h_samp_factor=%d\n", cinfo.comp_info[1].h_samp_factor);
    fprintf(stderr, "cinfo.comp_info[1].v_samp_factor=%d\n", cinfo.comp_info[1].v_samp_factor);
    fprintf(stderr, "cinfo.comp_info[1].downsampled_width=%d\n", cinfo.comp_info[1].downsampled_width);
    fprintf(stderr, "cinfo.comp_info[1].downsampled_height=%d\n", cinfo.comp_info[1].downsampled_height);
    fprintf(stderr, "cinfo.comp_info[2].h_samp_factor=%d\n", cinfo.comp_info[2].h_samp_factor);
    fprintf(stderr, "cinfo.comp_info[2].v_samp_factor=%d\n", cinfo.comp_info[2].v_samp_factor);
    fprintf(stderr, "cinfo.comp_info[2].downsampled_width=%d\n", cinfo.comp_info[2].downsampled_width);
    fprintf(stderr, "cinfo.comp_info[2].downsampled_height=%d\n", cinfo.comp_info[2].downsampled_height);
  }

  /* Note: Adjust default decompression parameters here. */

  /* 
   * By default, setting JCS_YCbCr produces 4:4:4 interleaved output. If input
   * is 4:2:2 and want 4:2:2 raw output, can set "cinfo.raw_data_out = TRUE", but
   * then must also use jpeg_read_raw_data().  Setup is very similar
   * to code in CJpeg.c.
   */
  cinfo.out_color_space = JCS_YCbCr; /* Should use jpeg_set_colorspace()? */

  cinfo.raw_data_out = TRUE;
  cinfo.do_fancy_upsampling = FALSE;

  /* Set quantization tables to standard tables if not already set.
     For example, some webcams produce jpegs lacking these tables,
     because it would be redundant and waste bandwidth to include
     them in every frame. */
  if (!cinfo.dc_huff_tbl_ptrs[0]) {
    cinfo.dc_huff_tbl_ptrs[0] = dc_huff_tbl_ptrs[0];
    cinfo.dc_huff_tbl_ptrs[1] = dc_huff_tbl_ptrs[1];
  }
    
  if (!cinfo.ac_huff_tbl_ptrs[0]) {
    cinfo.ac_huff_tbl_ptrs[0] = ac_huff_tbl_ptrs[0];
    cinfo.ac_huff_tbl_ptrs[1] = ac_huff_tbl_ptrs[1];
  }
	   
  /* I verified that setting .dct_method before jpeg_start_decompress() works with 
     jpeg-7 by adding printfs in the respective jidct*.c functions and running
     separate tests for dct_method ifast, islow, and float. */
  cinfo.dct_method = JDCT_DEFAULT; // priv->dct_method;
  (void) jpeg_start_decompress(&cinfo);

  uint8_t *buffers[3] = {};

  /* Allocate and use one of the output image buffer types, and fill
     that in during decompress.  Even if it isn't an assigned output,
     it will be needed for conversion to an assigned output. */
  if (fmt->imgtype == IMAGE_TYPE_YUV422P) {
    yuv422p = YUV422P_buffer_new(cinfo.image_width, cinfo.image_height, &jpeg_in->c);
    buffers[0] = yuv422p->y;
    buffers[1] = yuv422p->cb;
    buffers[2] = yuv422p->cr;
  }
  else if (fmt->imgtype == IMAGE_TYPE_YUV420P) {
    yuv420p = YUV420P_buffer_new(cinfo.image_width, cinfo.image_height, &jpeg_in->c);
    buffers[0] = yuv420p->y;
    buffers[1] = yuv420p->cb;
    buffers[2] = yuv420p->cr;
  }

  while (cinfo.output_scanline < cinfo.output_height) {
    int n;
    /* Setup necessary for raw downsampled data.  Note that depending on the
       format, each pass may produce 8 lines instead of 16, but the code
       here will still work since it uses cinfo.output_scanline, which is
       incremented by libjpeg.  */
    JSAMPROW y[16];
    JSAMPROW cb[16];
    JSAMPROW cr[16];
    for (n=0; n < 16; n++) {
      y[n] = buffers[0] + 
	((n+cinfo.output_scanline) * cinfo.image_width);
      cb[n] = buffers[1] + 
	((n+cinfo.output_scanline/fmt->crcb_height_divisor) * cinfo.image_width/fmt->crcb_width_divisor);
      cr[n] = buffers[2] + 
	((n+cinfo.output_scanline/fmt->crcb_height_divisor) * cinfo.image_width/fmt->crcb_width_divisor);
    }
      
    JSAMPARRAY array[3] = { y, cb, cr};
    JSAMPIMAGE image = array;
    /* Need to pass enough lines at a time, see 
        if (max_lines < lines_per_iMCU_row)
       test in jdapistd.c.  Sometimes only needs
       8, but 16 doesn't hurt in that case.  */

    n = jpeg_read_raw_data(&cinfo, image, 16);
    
    //printf("  n=%d cinfo.output_scanline=%d cinfo.output_height=%d\n", 
    //       n, cinfo.output_scanline , cinfo.output_height);

  }

  if (0) {
    /* Development testing: dump planes as .pgm files. */
    FILE *y = fopen("y.pgm", "wb");
    if (y) {
      fprintf(y, "P5\n%d %d\n255\n", cinfo.image_width, cinfo.image_height);
      fwrite(buffers[0], cinfo.image_width*cinfo.image_height, 1, y);
      fclose(y);
    }

    FILE *Cb = fopen("cb.pgm", "wb");
    if (Cb) {
      fprintf(Cb, "P5\n%d %d\n255\n", 
	      cinfo.image_width/fmt->crcb_width_divisor,
	      cinfo.image_height/fmt->crcb_height_divisor);
      fwrite(buffers[1], 
	     cinfo.image_width*cinfo.image_height/(fmt->crcb_width_divisor*fmt->crcb_height_divisor), 1, 
	     Cb);
      fclose(Cb);
    }

    FILE *Cr = fopen("cr.pgm", "wb");
    if (Cr) {
      fprintf(Cb, "P5\n%d %d\n255\n", 
	      cinfo.image_width/fmt->crcb_width_divisor,
	      cinfo.image_height/fmt->crcb_height_divisor);
      fwrite(buffers[2], 
	     cinfo.image_width*cinfo.image_height/(fmt->crcb_width_divisor*fmt->crcb_height_divisor), 1, 
	     Cr);
      fclose(Cr);
    }
  }

  (void) jpeg_finish_decompress(&cinfo);

 jdd:
  jpeg_destroy_decompress(&cinfo);

 out:
  if (yuv420p) {
    if (yuv420p_result) {
      *yuv420p_result = yuv420p;
    }
    else {
      YUV420P_buffer_discard(yuv420p);
    }
  }

  if (yuv422p) {
    if (yuv422p_result) {
      *yuv422p_result = yuv422p;
    }
    else {
      YUV422P_buffer_discard(yuv422p);
    }
  }
}


RGB3_buffer * Jpeg_to_rgb3(Jpeg_buffer * jpeg)
{
  RGB3_buffer * rgb3 = NULL;
  YUV422P_buffer * yuv422p = NULL;
  YUV420P_buffer * yuv420p = NULL;
  FormatInfo * fmt = NULL;
  Jpeg_decompress(jpeg, &yuv420p, &yuv422p, &fmt);
  if (yuv420p) {
    rgb3 = YUV420P_to_RGB3(yuv420p);
    YUV420P_buffer_discard(yuv420p);
  }
  else if (yuv422p) {
    rgb3 = YUV422P_to_RGB3(yuv422p);
    YUV422P_buffer_discard(yuv422p);
  }
  return rgb3;
}


/* I also put Jpeg_fix() in this module, because it has access to the huffman
   tables, and again I didn't want Images.c to pull in jpeglib.h, nor define
   a second copy of the tables. */
enum maker_enums { 
  SOI = 0xd8, 
  APP0 = 0xe0, 
  DQT = 0xdb, 
  SOF0 = 0xc0, 
  SOS = 0xda, 
  EOI = 0xd9, 
  DRI = 0xdd, 
  DHT = 0xc4 
};

static const char *markers[] = {
  [SOI] = "SOI",
  [APP0] = "APP0",
  [DQT] = "DQT", 
  [SOF0] = "SOF0",
  [SOS] = "SOS",
  [EOI] = "EOI",
  [DRI] = "DRI",
  [DHT] = "DHT",
};

void Jpeg_fix(Jpeg_buffer *jpeg)
{
  /* This is based on "mjpegfix.py" which I wrote several years
     earlier.  The Jpeg data is replaced at the end, but the buffer
     structure remains in place.  In other words, it does not produce
     a new Jpeg_buffer. */

  if (jpeg->has_huffman_tables) {
    return;
  }

  /* FIXME: Consider implementing and calling LockedRef_count() and
     returning if there are more then one reference holders. */

  int saw_sof = 0;
  int i = 0;
  int i0;
  int j;
  int len;
  ArrayU8 * newdata = ArrayU8_new();

  while (1) {
    uint8_t c0, c, marker;
    int blockL=0;
    int blockR=0;
    if (i == jpeg->encoded_length) {
      break;
    }
    c0 = jpeg->data[i];
    if (c0 != 0xff) {
      fprintf(stderr, "%d (0x%X): expected 0xff, saw 0x%02x\n", i, i, c0);
      break;
    }
    c = jpeg->data[i+1];
    if (!markers[c]) {
      fprintf(stderr, "Unknown marker: 0x%02x\n", c);
      i+=1;
      continue;
    }

    marker = c;

    if (marker == SOI) {
      i += 2;
    }
    else if (marker == SOS) {
      // Fixed size part, followed by variable length data...
      i0 = i+2;
      len = (jpeg->data[i+2]) << 8;
      len += jpeg->data[i+3];
      i += (len + 2);
      while (1) {
	uint8_t x[1] = { 0xff};
	j = ArrayU8_search(ArrayU8_temp_const(jpeg->data, jpeg->data_length),
			   i, 
			   ArrayU8_temp_const(x, 1));
	if (j == -1) {
	  fprintf(stderr, "reached end of data without finding closing tag!\n");
	  goto out;
	}
	if (jpeg->data[j+1] == 0x00) {
	  // Data contained literal 0xff.
	  i = j+1;
	  continue;
	}
	else if ( (jpeg->data[j+1] & 0xf0) == 0xd0 
		  && (jpeg->data[j+1] & 0x0f) <= 7) {
	  // Restart marker.  Keep searching inside SOS.
	  i = j+2;
	  continue;
	}
	else {
	  // Advance, keep searching...
	  i = j;
	  break;
	}
      }
      // block = jpegdata[i0:i];
      blockL = i0;
      blockR = i;
    }
    else if (marker == EOI) {
      uint8_t xx[2] = {c0, c};
      ArrayU8_append(newdata, ArrayU8_temp_const(xx, 2));
      goto out;
    }
    else if (marker == DRI) {
      // block = jpegdata[i+2:i+2+4];
      blockL = i+2;
      blockR = i+2+4;
      i += 6;
    }
    else {
      i0 = i+2;
      len = jpeg->data[i+2] << 8;
      len += jpeg->data[i+3];
      // print 'len=0x%x' % (len);
      i += (len + 2);
      // block = jpegdata[i0:i];
      blockL = i0;
      blockR = i;
    }
    
    if (saw_sof && marker != DHT) {
      /* Add huffman tables.  Using the same tables that are set
	 during decompression if they are found to be missing
	 there. */
      static const uint8_t hdr1[4] = {0xff, 0xc4, 0x00, 0x1f};
      static const uint8_t hdr2[4] = {0xff, 0xc4, 0x00, 0xb5};

      ArrayU8_append(newdata, ArrayU8_temp_const(hdr1, 4));
      ArrayU8_append(newdata, ArrayU8_temp_const(dc_huff_tbl_ptrs[0]->bits, 17));
      ArrayU8_append(newdata, ArrayU8_temp_const(dc_huff_tbl_ptrs[0]->huffval, 12));

      ArrayU8_append(newdata, ArrayU8_temp_const(hdr2, 4));
      ArrayU8_append(newdata, ArrayU8_temp_const(ac_huff_tbl_ptrs[0]->bits, 17));
      ArrayU8_append(newdata, ArrayU8_temp_const(ac_huff_tbl_ptrs[0]->huffval, 162));

      ArrayU8_append(newdata, ArrayU8_temp_const(hdr1, 4));
      ArrayU8_append(newdata, ArrayU8_temp_const(dc_huff_tbl_ptrs[1]->bits, 17));
      ArrayU8_append(newdata, ArrayU8_temp_const(dc_huff_tbl_ptrs[1]->huffval, 12));

      ArrayU8_append(newdata, ArrayU8_temp_const(hdr2, 4));
      ArrayU8_append(newdata, ArrayU8_temp_const(ac_huff_tbl_ptrs[1]->bits, 17));
      ArrayU8_append(newdata, ArrayU8_temp_const(ac_huff_tbl_ptrs[1]->huffval, 162));
    }
    
    if (marker == SOF0) {
      saw_sof = 1;
    }
    else {
      saw_sof = 0;
    }

    uint8_t xx[2] = {c0, c};
    ArrayU8_append(newdata, ArrayU8_temp_const(xx, 2));

    if (blockL < blockR) {
      ArrayU8_append(newdata, ArrayU8_temp_const(jpeg->data+blockL, (blockR-blockL)));
    }
  }

 out:
  /* Transfer newdata->data to jpeg->data.  This requires a little "manual" effort. */
  Mem_free(jpeg->data);
  jpeg->data = newdata->data;
  jpeg->data_length = newdata->available;
  jpeg->encoded_length = newdata->len;
  jpeg->has_huffman_tables = 1;
  newdata->data = NULL;
  ArrayU8_cleanup(&newdata);
}
