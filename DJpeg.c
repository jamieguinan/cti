#include <stdio.h>
#include <string.h>
#include <sys/time.h>		/* gettimeofday */
#include <setjmp.h>

#include "jpeglib.h"

#include "cdjpeg.h"
#include "wrmem.h"
#include "jmemsrc.h"
#include "jpeghufftables.h"

#include "CTI.h"
#include "DJpeg.h"
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"

static void Config_handler(Instance *pi, void *data);
static void Jpeg_handler(Instance *pi, void *data);

/* DJpeg Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_JPEG };
static Input DJpeg_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = Jpeg_handler },
};

enum { OUTPUT_RGB3, OUTPUT_GRAY, OUTPUT_422P };
static Output DJpeg_outputs[] = { 
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = {.type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_422P ] = {.type_label = "422P_buffer", .destination = 0L },
};

typedef struct {
  /* Jpeg decode context... */
  int use_green_for_gray;
  int sampling_warned;
} 
DJpeg_private;

static void jerr_warning_noop(j_common_ptr cinfo, int msg_level)
{
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


static int do_quit(Instance *pi, const char *value)
{
  exit(0);
  return 0;
}


static Config config_table[] = {
  { "quit",    do_quit, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Jpeg_handler(Instance *pi, void *data)
{
  DJpeg_private *priv = pi->data;
  struct timeval t1, t2;
  int save_width = 0;
  int save_height = 0;
  Jpeg_buffer *jpeg_in = data;
  int gray_handled = 0;

  gettimeofday(&t1, 0L);

  /* Decompress for one or more outputs.   Don't worry about the redundant code in the blocks,
     putting it in functions or a loop would be even more confusing... */

  if (pi->outputs[OUTPUT_RGB3].destination) {
    /* Decompress input buffer.  See "libjpeg.txt" in IJPEG source, and "djpeg.c".  */
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    djpeg_dest_ptr dest_mgr = NULL;
    JDIMENSION num_scanlines;
    RGB3_buffer *rgb_out = 0L;
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
      printf("%s:%d\n", __func__, __LINE__);
      FILE *f = fopen("error.jpg", "wb");
      int n = fwrite(jpeg_in->data, 1, jpeg_in->data_length, f);
      if (n != jpeg_in->data_length) {
	perror("fwrite"); 
      } else {
	printf("saved erroneous jpeg to error.jpg\n");
	fclose(f);
      }
      goto check_errors_1;
    }

    jpeg_mem_src(&cinfo, jpeg_in->data, jpeg_in->data_length);

    (void) jpeg_read_header(&cinfo, TRUE);

    rgb_out = RGB3_buffer_new(cinfo.image_width, cinfo.image_height);

    /* Note: Adjust default decompression parameters here.  */

    /* Set quantization tables if not already set.  For example, the
       Logitech Quickcam Pro 9000 models produce Jpegs lacking these
       tables. */
    if (!cinfo.dc_huff_tbl_ptrs[0]) {
      cinfo.dc_huff_tbl_ptrs[0] = dc_huff_tbl_ptrs[0];
      cinfo.dc_huff_tbl_ptrs[1] = dc_huff_tbl_ptrs[1];
    }

    if (!cinfo.ac_huff_tbl_ptrs[0]) {
      cinfo.ac_huff_tbl_ptrs[0] = ac_huff_tbl_ptrs[0];
      cinfo.ac_huff_tbl_ptrs[1] = ac_huff_tbl_ptrs[1];
    }

    dest_mgr = jinit_write_mem(&cinfo, rgb_out->data, rgb_out->data_length);

    (void) jpeg_start_decompress(&cinfo);

    /* Note: I'm not sure if this gets overridden.  It definitely does in the 
       422p case below.  Anyway, should set desired JDCT_DEFAULT in jconfig.h. */
    cinfo.dct_method = JDCT_FLOAT;

    (*dest_mgr->start_output) (&cinfo, dest_mgr);

    /* Decompression loop: */
    while (cinfo.output_scanline < cinfo.output_height) {
      num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,
					  dest_mgr->buffer_height);
      (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
    }

  check_errors_1:

    if (!error) {
      /* The order here looks weird, but this is how djpeg.c does it... */
      (*dest_mgr->finish_output) (&cinfo, dest_mgr);
      (void) jpeg_finish_decompress(&cinfo);

      rgb_out->tv = jpeg_in->tv;

      if (pi->outputs[OUTPUT_GRAY].destination && priv->use_green_for_gray) {
	int k;
	// printf("green_for_gray\n");
	Gray_buffer *gray_out = Gray_buffer_new(cinfo.image_width, cinfo.image_height);
	for (k=0; k < gray_out->data_length; k++) {
	  gray_out->data[k] = rgb_out->data[k*3+1];
	}
	PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
	gray_handled = 1;
      }
      
      PostData(rgb_out, pi->outputs[OUTPUT_RGB3].destination);
    }
    else {
      RGB3_buffer_discard(rgb_out);
    }

    jpeg_destroy_decompress(&cinfo);
  }

  if (pi->outputs[OUTPUT_422P].destination
      || (pi->outputs[OUTPUT_GRAY].destination && !gray_handled)) {
    /* Decompress to YCbCr, and copy Y channel to Gray buffer if needed. */
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    Y422P_buffer * y422p_out = 0L;
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
      printf("%s:%d\n", __func__, __LINE__);
      goto check_errors_2;
    }

    cinfo.raw_data_out = TRUE;

    jpeg_mem_src(&cinfo, jpeg_in->data, jpeg_in->data_length);

    (void) jpeg_read_header(&cinfo, TRUE);

    y422p_out = Y422P_buffer_new(cinfo.image_width, cinfo.image_height);

    save_width = cinfo.image_width;
    save_height = cinfo.image_height;

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

#if 0
    printf("note: jpeg colorspace is %s\n",
	   cinfo.jpeg_color_space == JCS_GRAYSCALE ? "JCS_GRAYSCALE" :
	   cinfo.jpeg_color_space == JCS_RGB ? "JCS_RGB" :
	   cinfo.jpeg_color_space == JCS_YCbCr ? "JCS_YCbCr" :
	   "unknown");
#endif

    /* Verify sampling factors, can only handle YCbCr 4:2:2 data here. */
    if (cinfo.jpeg_color_space == JCS_YCbCr &&
	cinfo.comp_info[0].h_samp_factor == 2 &&
	cinfo.comp_info[0].v_samp_factor == 2 &&
	cinfo.comp_info[1].h_samp_factor == 1 &&
	cinfo.comp_info[1].v_samp_factor == 2 &&
	cinfo.comp_info[2].h_samp_factor == 1 &&
	cinfo.comp_info[2].v_samp_factor == 2) {
      /* Ok. */
    }
    else {
      if (!priv->sampling_warned) {
	fprintf(stderr, "jpeg data is not 4:2:2!  This could be handled, but code is not written for it...\n");
	fprintf(stderr, "cinfo.comp_info[0].h_samp_factor=%d\n", cinfo.comp_info[0].h_samp_factor);
	fprintf(stderr, "cinfo.comp_info[0].v_samp_factor=%d\n", cinfo.comp_info[0].v_samp_factor);
	fprintf(stderr, "cinfo.comp_info[1].h_samp_factor=%d\n", cinfo.comp_info[1].h_samp_factor);
	fprintf(stderr, "cinfo.comp_info[1].v_samp_factor=%d\n", cinfo.comp_info[1].v_samp_factor);
	fprintf(stderr, "cinfo.comp_info[2].h_samp_factor=%d\n", cinfo.comp_info[2].h_samp_factor);
	fprintf(stderr, "cinfo.comp_info[2].v_samp_factor=%d\n", cinfo.comp_info[2].v_samp_factor);
	priv->sampling_warned = 1;
      }
    }


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
	   
    (void) jpeg_start_decompress(&cinfo);

    /* Note: setting cinfo.dct_method here does not make any differnce, libjpeg 
       resets it in the jpeg_consume_input()default_decompress_parms() path. 
       Solution is to #define JDCT_DEFAULT it to desired value in jconfig.h */

    uint8_t *buffers[3] = { y422p_out->y, y422p_out->cb, y422p_out->cr};
    // printf("cinfo.output_scanline=%d cinfo.output_height=%d\n", cinfo.output_scanline , cinfo.output_height);
    while (cinfo.output_scanline < cinfo.output_height) {
      int n;
      /* Setup necessary for raw downsampled data.  */
      JSAMPROW y[16];
      JSAMPROW cb[16];
      JSAMPROW cr[16];
      for (n=0; n < 16; n++) {
	y[n] = buffers[0] + ((cinfo.output_scanline+n)* cinfo.image_width);
	cb[n] = buffers[1] + ((cinfo.output_scanline+n) * cinfo.image_width / 2);
	cr[n] = buffers[2] + ((cinfo.output_scanline+n) * cinfo.image_width / 2);
      }
      
      JSAMPARRAY array[3] = { y, cb, cr};
      JSAMPIMAGE image = array;
      /* Need to pass enough lines at a time, see "(num_lines < lines_per_iMCU_row)" test in
	 jcapistd.c */
      n = jpeg_read_raw_data(&cinfo, image, 16);
      //printf("  n=%d cinfo.output_scanline=%d cinfo.output_height=%d\n", 
      //n, cinfo.output_scanline , cinfo.output_height);
    }
    

  check_errors_2:
    if (!error) {
      (void) jpeg_finish_decompress(&cinfo);
      
      y422p_out->tv = jpeg_in->tv;
      
      if (pi->outputs[OUTPUT_GRAY].destination && !gray_handled) {
	/* Clone Y channel, pass along. */
	Gray_buffer *gray_out = Gray_buffer_new(cinfo.image_width, cinfo.image_height);
	memcpy(gray_out->data, y422p_out->y, y422p_out->y_length);
	PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
      }
      
      if (pi->outputs[OUTPUT_422P].destination) {
	PostData(y422p_out, pi->outputs[OUTPUT_422P].destination);
      }
      else {
	/* Only wanted gray output, discard the ycrcb buffer */
	Y422P_buffer_discard(y422p_out);
      }
    }
    else {
      Y422P_buffer_discard(y422p_out);
    }
    
    jpeg_destroy_decompress(&cinfo);
  }

  /* Discard input buffer. */
  Jpeg_buffer_discard(jpeg_in);
  pi->counter += 1;

  /* Calculate decompress time. */
  gettimeofday(&t2, 0L);
  float tdiff =  (t2.tv_sec + t2.tv_usec/1000000.0) - (t1.tv_sec + t1.tv_usec/1000000.0);

  if (0) { 
    printf("%.5f (%dx%d)\n", tdiff, save_width, save_height);
  }
}

static void DJpeg_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }
}

static void DJpeg_instance_init(Instance *pi)
{
  DJpeg_private *priv = Mem_calloc(1, sizeof(*priv));
  priv->use_green_for_gray = 1;
  pi->data = priv;
}

static Template DJpeg_template = {
  .label = "DJpeg",
  .inputs = DJpeg_inputs,
  .num_inputs = table_size(DJpeg_inputs),
  .outputs = DJpeg_outputs,
  .num_outputs = table_size(DJpeg_outputs),
  .tick = DJpeg_tick,
  .instance_init = DJpeg_instance_init,
};


void DJpeg_init(void)
{
  Template_register(&DJpeg_template);
}


/* Create a Jpeg buffer from raw jpeg data.  This can return NULL if
   invalid jpeg data!  I put this function in this file rather than
   Images.c because this has the jpeglib.h header, and if I'm building
   on a system without the jpeg library, I still want Images.c to
   build. */
Jpeg_buffer *Jpeg_buffer_from(uint8_t *data, int data_length)
{
  Jpeg_buffer *jpeg = 0L;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jmp_buf jb;
  int error = 0;

  cinfo.err = jpeg_std_error(&jerr); /* NOTE: See ERREXIT, error_exit, 
					this may cause the program to call exit()! */
  jerr.error_exit = jerr_error_handler;

  jpeg_create_decompress(&cinfo);

  cinfo.client_data = &jb;
  error = setjmp(jb);
  if (error) {
    {
      printf("%s:%d\n", __func__, __LINE__);
      FILE *f = fopen("error.jpg", "wb");
      int n = fwrite(data, 1, data_length, f);
      if (n != data_length) {
	perror("fwrite"); 
      } else {
	printf("saved erroneous jpeg to error.jpg\n");
	fclose(f);
      }
    }
    goto out;
  }

  jpeg_mem_src(&cinfo, data, data_length);

  (void) jpeg_read_header(&cinfo, TRUE); /* Possible path to setjmp() above. */

  // printf("jpeg: width=%d height=%d\n", cinfo.image_width, cinfo.image_height);

  /* If no error, allocate and return jpeg, otherwise it will return NULL. */
  jpeg = Jpeg_buffer_new(data_length);
  jpeg->width = cinfo.image_width;
  jpeg->height = cinfo.image_height;
  jpeg->data_length = data_length;
  jpeg->encoded_length = data_length;
  memcpy(jpeg->data, data, data_length);

 out:  
  jpeg_destroy_decompress(&cinfo); 
  
  return jpeg;
}

