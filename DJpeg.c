#include <stdio.h>
#include <string.h>
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

enum { OUTPUT_RGB3, OUTPUT_GRAY, OUTPUT_YUV422P, OUTPUT_YUV420P, OUTPUT_JPEG };
static Output DJpeg_outputs[] = { 
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = {.type_label = "GRAY_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_JPEG ] = {.type_label = "Jpeg_buffer", .destination = 0L }, /* pass-through */
};

typedef struct {
  Instance i;

  /* Jpeg decode context... */
  int use_green_for_gray;
  int sampling_warned;
  int max_messages;
  int dct_method;

  int every;
} 
DJpeg_private;


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


static int set_dct_method(Instance *pi, const char *value)
{
  DJpeg_private *priv = (DJpeg_private *)pi;

  if (streq(value, "islow")) {
    priv->dct_method = JDCT_ISLOW;
  }
  else if (streq(value, "ifast")) {
    priv->dct_method = JDCT_IFAST;
  }
  else if (streq(value, "float")) {
    priv->dct_method = JDCT_FLOAT;
  }
  else {
    fprintf(stderr, "%s: unknown method %s\n", __func__, value);
  }
  return 0;
}


static int do_quit(Instance *pi, const char *value)
{
  exit(0);
  return 0;
}


static Config config_table[] = {
  { "max_messages", 0L, 0L, 0L, cti_set_int, offsetof(DJpeg_private, max_messages) },
  { "every", 0L, 0L, 0L, cti_set_int, offsetof(DJpeg_private, every) },
  { "dct_method", set_dct_method, 0L, 0L},
  { "quit",    do_quit, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


typedef struct {
  const char *label;
  ImageType imgtype;
  int libjpeg_colorspace;
  const char * libjpeg_colorspace_label;
  int factors[6];
  int crcb_width_divisor;
  int crcb_height_divisor;
} FormatInfo;

static FormatInfo known_formats[] = {
  /* See swdev/notes.txt regarding subsampling and formats. */
  { .imgtype = IMAGE_TYPE_YUV420P, .label = "YUV420P", 
    .libjpeg_colorspace = JCS_YCbCr, .libjpeg_colorspace_label = "JCS_YCbCr",
    .factors = { 2, 2, 1, 1, 1, 1}, .crcb_width_divisor = 2, .crcb_height_divisor = 2},

  { .imgtype = IMAGE_TYPE_YUV422P, .label = "YUV422P", 
    .libjpeg_colorspace = JCS_YCbCr, .libjpeg_colorspace_label = "JCS_YCbCr",
    .factors = { 2, 1, 1, 1, 1, 1}, .crcb_width_divisor = 2, .crcb_height_divisor = 1},
};

static void Jpeg_handler(Instance *pi, void *data)
{
  DJpeg_private *priv = (DJpeg_private *)pi;
  double t1, t2;
  int save_width = 0;
  int save_height = 0;
  Jpeg_buffer *jpeg_in = data;
  int i;

  /* Provisional image buffers. */
  YUV422P_buffer * yuv422p = NULL;
  YUV420P_buffer * yuv420p = NULL;
  RGB3_buffer * rgb3 = NULL;

  if (priv->every && (pi->counter % priv->every != 0)) {
    goto out;
  }

  if (priv->max_messages && pi->pending_messages > priv->max_messages) {
    /* Skip without decoding. */
    fprintf(stderr, "DJpeg skipping %d (%d %d)\n", pi->counter,
	    pi->pending_messages, priv->max_messages);
    goto out;
  }

  getdoubletime(&t1);

  if (!(pi->outputs[OUTPUT_YUV422P].destination ||
	pi->outputs[OUTPUT_YUV420P].destination ||
	pi->outputs[OUTPUT_RGB3].destination ||
	pi->outputs[OUTPUT_GRAY].destination)) {
    /* No decompressed outputs set up. */
    goto out;
  }


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
    printf("%s:%d\n", __func__, __LINE__);
    goto jdd;
  }
  
  cinfo.raw_data_out = TRUE;

  jpeg_mem_src(&cinfo, jpeg_in->data, jpeg_in->data_length);

  (void) jpeg_read_header(&cinfo, TRUE);

  save_width = cinfo.image_width;
  save_height = cinfo.image_height;
  
  int samp_factors[6] = {
    cinfo.comp_info[0].h_samp_factor,
    cinfo.comp_info[0].v_samp_factor,
    cinfo.comp_info[1].h_samp_factor,
    cinfo.comp_info[1].v_samp_factor,
    cinfo.comp_info[2].h_samp_factor,
    cinfo.comp_info[2].v_samp_factor,
  };

  FormatInfo *fmt = NULL;
  for (i=0; i < cti_table_size(known_formats); i++) {
    if (memcmp(samp_factors, known_formats[i].factors, sizeof(samp_factors)) == 0) {
      // printf("Jpeg subsampling: %s\n", known_formats[i].label);
      fmt = &(known_formats[i]);
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
  cinfo.dct_method = priv->dct_method;
  (void) jpeg_start_decompress(&cinfo);

  uint8_t *buffers[3] = {};

  /* Rather than allocate separate buffers, allocate and use one of
     the output image buffer types, and fill that in during
     decompress.  Even if it isn't an assigned output, it will be
     needed for conversion to an assigned output. */
  if (fmt->imgtype == IMAGE_TYPE_YUV422P) {
    /* FIXME: Pad for DCT block boundaries, see libjpeg.txt */
    yuv422p = YUV422P_buffer_new(cinfo.image_width, cinfo.image_height, &jpeg_in->c);
    buffers[0] = yuv422p->y;
    buffers[1] = yuv422p->cb;
    buffers[2] = yuv422p->cr;
  }
  else if (fmt->imgtype == IMAGE_TYPE_YUV420P) {
    /* FIXME: Pad for DCT block boundaries, see libjpeg.txt */
    yuv420p = YUV420P_buffer_new(cinfo.image_width, cinfo.image_height, &jpeg_in->c);
    buffers[0] = yuv420p->y;
    buffers[1] = yuv420p->cb;
    buffers[2] = yuv420p->cr;
  }
  

  while (cinfo.output_scanline < cinfo.output_height) {
    int n;
    /* Setup necessary for raw downsampled data.  Note that depending on the
       format, each pass may produced 8 lines instead of 16, but the code
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
    /* Need to pass enough lines at a time, see "(num_lines <
       lines_per_iMCU_row)" test in jcapistd.c.  Sometimes only needs
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

  /* Sanity check... */
  if (!yuv422p && !yuv420p) {
    printf("neither yuv422p or yuv420p is set!?\n");
    goto jdd;
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    /* Clone Y channel, pass along. */
    Gray_buffer *gray_out = Gray_buffer_new(cinfo.image_width, cinfo.image_height, &jpeg_in->c);
    memcpy(gray_out->data, buffers[0], cinfo.image_width * cinfo.image_height);
    PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
  }

  if (pi->outputs[OUTPUT_YUV422P].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV420P && !yuv422p) {
      /* Convert. */
      yuv422p = YUV420P_to_YUV422P(yuv420p);
    }
    /* Post. */
    PostData(YUV422P_buffer_ref(yuv422p), pi->outputs[OUTPUT_YUV422P].destination);
  }

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV422P && !yuv420p) {
      /* Convert. */
      yuv420p = YUV422P_to_YUV420P(yuv422p);
    }
    /* Post. */
    PostData(YUV420P_buffer_ref(yuv420p), pi->outputs[OUTPUT_YUV420P].destination);
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    if (fmt->imgtype == IMAGE_TYPE_YUV420P) {
      rgb3 = YUV420P_to_RGB3(yuv420p);
    }
    else if (fmt->imgtype == IMAGE_TYPE_YUV422P) {
      rgb3 = YUV422P_to_RGB3(yuv422p);
    }
    /* Post... */
    PostData(RGB3_buffer_ref(rgb3), pi->outputs[OUTPUT_RGB3].destination);
  }

  /* Discard/unref buffers. */
  if (yuv422p) {
    YUV422P_buffer_discard(yuv422p);
  }

  if (yuv420p) {
    YUV420P_buffer_discard(yuv420p);
  }

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }


 jdd:
  jpeg_destroy_decompress(&cinfo);

 out:
  /* Discard or pass along input buffer. */
  if (pi->outputs[OUTPUT_JPEG].destination) {
    PostData(jpeg_in, pi->outputs[OUTPUT_JPEG].destination);
  }
  else {
    Jpeg_buffer_discard(jpeg_in);
  }
  pi->counter += 1;

  /* Calculate decompress time. */
  getdoubletime(&t2);
  float tdiff = t2 - t1;

  dpf("djpeg %.5f (%dx%d)\n", tdiff, save_width, save_height);
}

static void DJpeg_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}

static void DJpeg_instance_init(Instance *pi)
{
  DJpeg_private *priv = (DJpeg_private *)pi;
  priv->use_green_for_gray = 1;
  priv->dct_method = JDCT_DEFAULT;
}

static Template DJpeg_template = {
  .label = "DJpeg",
  .priv_size = sizeof(DJpeg_private),
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

  cinfo.err = jpeg_std_error(&jerr); /* NOTE: See ERREXIT, error_exit, 
					this may cause the program to call exit()! */
  jerr.error_exit = jerr_error_handler;

  jpeg_create_decompress(&cinfo);

  cinfo.client_data = &jb;
  error = setjmp(jb);
  if (error) {
    printf("%s:%d\n", __func__, __LINE__);
    save_error_jpeg(data, data_length);
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

