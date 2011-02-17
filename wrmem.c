/*
 * wrmem.c: output decompressed data to memory buffer.
 * Modifications (C) 2010 Jamie Guinan.
 *
 * Derived from wrppm.c, original copyright follows:
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * Modified 2009 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to write output images in PPM/PGM format.
 * The extended 2-byte-per-sample raw PPM/PGM formats are supported.
 * The PBMPLUS library is NOT required to compile this software
 * (but it is highly useful as a set of PPM image manipulation programs).
 *
 * These routines may need modification for non-Unix environments or
 * specialized applications.  As they stand, they assume output to
 * an ordinary stdio stream.
 */

#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */

#include "wrmem.h"

/*
 * For 12-bit JPEG data, we either downscale the values to 8 bits
 * (to write standard byte-per-sample PPM/PGM files), or output
 * nonstandard word-per-sample PPM/PGM files.  Downscaling is done
 * if PPM_NORAWWORD is defined (this can be done in the Makefile
 * or in jconfig.h).
 * (When the core library supports data precision reduction, a cleaner
 * implementation will be to ask for that instead.)
 */

#if BITS_IN_JSAMPLE == 8
#define PUTPPMSAMPLE(ptr,v)  *ptr++ = (char) (v)
#define BYTESPERSAMPLE 1
#define PPM_MAXVAL 255
#else
#ifdef PPM_NORAWWORD
#define PUTPPMSAMPLE(ptr,v)  *ptr++ = (char) ((v) >> (BITS_IN_JSAMPLE-8))
#define BYTESPERSAMPLE 1
#define PPM_MAXVAL 255
#else
/* The word-per-sample format always puts the MSB first. */
#define PUTPPMSAMPLE(ptr,v)			\
	{ register int val_ = v;		\
	  *ptr++ = (char) ((val_ >> 8) & 0xFF);	\
	  *ptr++ = (char) (val_ & 0xFF);	\
	}
#define BYTESPERSAMPLE 2
#define PPM_MAXVAL ((1<<BITS_IN_JSAMPLE)-1)
#endif
#endif


/*
 * When JSAMPLE is the same size as char, we can just fwrite() the
 * decompressed data to the PPM or PGM file.  On PCs, in order to make this
 * work the output buffer must be allocated in near data space, because we are
 * assuming small-data memory model wherein fwrite() can't reach far memory.
 * If you need to process very wide images on a PC, you might have to compile
 * in large-memory model, or else replace fwrite() with a putc() loop ---
 * which will be much slower.
 */


/* Private version of data destination object */

typedef struct {
  struct djpeg_dest_struct pub;	/* public fields */

  /* Usually these two pointers point to the same place: */
  char *iobuffer;		/* fwrite's I/O buffer */
  JSAMPROW pixrow;		/* decompressor output buffer */
  size_t buffer_width;		/* width of I/O buffer */
  JDIMENSION samples_per_row;	/* JSAMPLEs per output row */

  JOCTET *output_buffer;
  int output_length;
  int output_offset;
} mem_dest_struct;

typedef mem_dest_struct * mem_dest_ptr;

/* All the output calls are the same.   I threaded them into here, where I
 * check for buffer overruns.
 */
static void dest_output(mem_dest_ptr dest)
{
  int bytes_remaining = dest->output_length - dest->output_offset;

  if (bytes_remaining < dest->buffer_width) {
    fprintf(stderr, "%s: buffer overrun averted!\n", __func__);
    return;
  }
  memcpy(dest->output_buffer + dest->output_offset, dest->iobuffer, dest->buffer_width);
  dest->output_offset += dest->buffer_width;
}

/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 *
 * put_pixel_rows handles the "normal" 8-bit case where the decompressor
 * output buffer is physically the same as the fwrite buffer.
 */

METHODDEF(void)
put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		JDIMENSION rows_supplied)
{
  mem_dest_ptr dest = (mem_dest_ptr) dinfo;

  dest_output(dest);
}


/*
 * This code is used when we have to copy the data and apply a pixel
 * format translation.  Typically this only happens in 12-bit mode.
 */

METHODDEF(void)
copy_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		 JDIMENSION rows_supplied)
{
  mem_dest_ptr dest = (mem_dest_ptr) dinfo;
  register char * bufferptr;
  register JSAMPROW ptr;
  register JDIMENSION col;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = dest->samples_per_row; col > 0; col--) {
    PUTPPMSAMPLE(bufferptr, GETJSAMPLE(*ptr++));
  }

  dest_output(dest);
}


/*
 * Write some pixel data when color quantization is in effect.
 * We have to demap the color index values to straight data.
 */

METHODDEF(void)
put_demapped_rgb (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		  JDIMENSION rows_supplied)
{
  mem_dest_ptr dest = (mem_dest_ptr) dinfo;
  register char * bufferptr;
  register int pixval;
  register JSAMPROW ptr;
  register JSAMPROW color_map0 = cinfo->colormap[0];
  register JSAMPROW color_map1 = cinfo->colormap[1];
  register JSAMPROW color_map2 = cinfo->colormap[2];
  register JDIMENSION col;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = cinfo->output_width; col > 0; col--) {
    pixval = GETJSAMPLE(*ptr++);
    PUTPPMSAMPLE(bufferptr, GETJSAMPLE(color_map0[pixval]));
    PUTPPMSAMPLE(bufferptr, GETJSAMPLE(color_map1[pixval]));
    PUTPPMSAMPLE(bufferptr, GETJSAMPLE(color_map2[pixval]));
  }
  
  dest_output(dest);
}


METHODDEF(void)
put_demapped_gray (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		   JDIMENSION rows_supplied)
{
  mem_dest_ptr dest = (mem_dest_ptr) dinfo;
  register char * bufferptr;
  register JSAMPROW ptr;
  register JSAMPROW color_map = cinfo->colormap[0];
  register JDIMENSION col;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = cinfo->output_width; col > 0; col--) {
    PUTPPMSAMPLE(bufferptr, GETJSAMPLE(color_map[GETJSAMPLE(*ptr++)]));
  }

  dest_output(dest);
}


METHODDEF(void)
start_output_mem (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  /* No-op. */
}


METHODDEF(void)
finish_output_mem (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  /* No-op. */
}


/*
 * The module selection routine for memory output.
 */

GLOBAL(djpeg_dest_ptr)
jinit_write_mem (j_decompress_ptr cinfo, JOCTET *buffer, int buffer_length)
{
  mem_dest_ptr dest;

  /* Create module interface object, fill in method pointers */
  dest = (mem_dest_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(mem_dest_struct));
  dest->pub.start_output = start_output_mem;
  dest->pub.finish_output = finish_output_mem;

  /* Calculate output image dimensions so we can allocate space */
  jpeg_calc_output_dimensions(cinfo);

  /* Create physical I/O buffer.  Note we make this near on a PC. */
  dest->samples_per_row = cinfo->output_width * cinfo->out_color_components;
  dest->buffer_width = dest->samples_per_row * (BYTESPERSAMPLE * SIZEOF(char));
  dest->iobuffer = (char *) (*cinfo->mem->alloc_small)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, dest->buffer_width);

  if (cinfo->quantize_colors || BITS_IN_JSAMPLE != 8 ||
      SIZEOF(JSAMPLE) != SIZEOF(char)) {
    /* When quantizing, we need an output buffer for colormap indexes
     * that's separate from the physical I/O buffer.  We also need a
     * separate buffer if pixel format translation must take place.
     */
    dest->pub.buffer = (*cinfo->mem->alloc_sarray)
      ((j_common_ptr) cinfo, JPOOL_IMAGE,
       cinfo->output_width * cinfo->output_components, (JDIMENSION) 1);
    dest->pub.buffer_height = 1;
    if (! cinfo->quantize_colors)
      dest->pub.put_pixel_rows = copy_pixel_rows;
    else if (cinfo->out_color_space == JCS_GRAYSCALE)
      dest->pub.put_pixel_rows = put_demapped_gray;
    else
      dest->pub.put_pixel_rows = put_demapped_rgb;
  } else {
    /* We will copy directly from decompressor output buffer. */
    /* Synthesize a JSAMPARRAY pointer structure */
    /* Cast here implies near->far pointer conversion on PCs */
    dest->pixrow = (JSAMPROW) dest->iobuffer;
    dest->pub.buffer = & dest->pixrow;
    dest->pub.buffer_height = 1;
    dest->pub.put_pixel_rows = put_pixel_rows;
  }

  dest->output_buffer = buffer;
  dest->output_length = buffer_length;
  dest->output_offset = 0;

  return (djpeg_dest_ptr) dest;
}
