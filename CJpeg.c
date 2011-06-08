#include <stdio.h>
#include <string.h>
#include <sys/time.h>		/* gettimeofday */

#include "jpeglib.h"

#include "cdjpeg.h"
#include "jmemdst.h"

#include "CTI.h"
#include "CJpeg.h"
#include "Images.h"
#include "Mem.h"
#include "Cfg.h"
#include "Log.h"

#define _min(a, b)  ((a) < (b) ? (a) : (b))
#define _max(a, b)  ((a) > (b) ? (a) : (b))

static void Config_handler(Instance *pi, void *msg);
static void rgb3_handler(Instance *pi, void *msg);
static void bgr3_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

/* CJpeg Instance and Template implementation. */
enum { INPUT_CONFIG, INPUT_RGB3, INPUT_BGR3, INPUT_422P };
static Input CJpeg_inputs[] = { 
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = rgb3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = bgr3_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_JPEG };
static Output CJpeg_outputs[] = { 
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
};

typedef struct {
  /* Jpeg encode context... */
  int quality;
  int adjusted_quality;
  float time_limit;
} CJpeg_private;


static int set_quality(Instance *pi, const char *value)
{
  CJpeg_private *priv = pi->data;
  int quality = atoi(value);

  if (50 <= quality && quality <= 100) {
    priv->quality = quality;
    priv->adjusted_quality = quality;
    return 0;
  }
  else {
    return -1;
  }
}

static void get_quality(Instance *pi, Value *v)
{
  CJpeg_private *priv = pi->data;
  v->u.int_value = priv->quality;
}

static void get_quality_range(Instance *pi, Range *r)
{
  r->u.ints.min = 50;
  r->u.ints.max = 100;
  r->u.ints.step = 1;
}

static Config config_table[] = {
  { "quality",    set_quality, get_quality, get_quality_range },
};

/// extern int jchuff_verbose;

static void compress_and_post(Instance *pi, 
			      int width, int height,
			      uint8_t *c1, uint8_t *c2, uint8_t *c3)
{
  /* Compress input buffer.  See "libjpeg.txt" in IJPEG source, and "cjpeg.c". */
  CJpeg_private *priv = pi->data;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  Jpeg_buffer *jpeg_out = 0L;
  struct timeval t1, t2;
  int report_time = 0;

  gettimeofday(&t1, 0L);

  cinfo.err = jpeg_std_error(&jerr); /* NOTE: See ERREXIT, error_exit, 
					this may cause the program to call exit()! */
  jpeg_create_compress(&cinfo);

  /*  Leave enough space for 100% of original size, plus some header. */
  jpeg_out = Jpeg_buffer_new(width*height*3+16384); 
  jpeg_out->width = width;
  jpeg_out->height = height;

  if (c2 && c3) {
    int w2 = (width/8)*8;
    int h2 = (height/8)*8;

    // DEBUGGING 
    if (w2 != width || h2 != height) {
      fprintf(stderr, 
	      "width and/or height is off:\n");
      fflush(stderr);
      Log_dump();
      backtrace_and_exit();
    }

    if (w2 != width) {
      fprintf(stderr, "warning: truncating width from %d to %d\n", width, w2);
      jpeg_out->width = w2;
    }
    if (h2 != height) {
      fprintf(stderr, "warning: truncating height from %d to %d\n", height, h2);
      jpeg_out->height = h2;
    }
    // jpeg_out->tv = y422p_in->tv;
  }

  jpeg_mem_dest(&cinfo, jpeg_out->data, jpeg_out->data_length, &jpeg_out->encoded_length);

  /* NOTE: It turns out there is actually no need for jinit_read_mem()
     [rdmem.c], just set the pointers in the encode loop! */

  cinfo.image_width = jpeg_out->width;
  cinfo.image_height = jpeg_out->height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB; /* reset below if y422p */

  jpeg_set_defaults(&cinfo);

  if (c2 && c3) {
    /* See "Raw (downsampled) image data" section in libjpeg.txt. */
    cinfo.raw_data_in = TRUE;
    jpeg_set_colorspace(&cinfo, JCS_YCbCr);
      
    cinfo.do_fancy_downsampling = FALSE;  // http://www.lavrsen.dk/svn/motion/trunk/picture.c

    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;

    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 2;

    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 2;
  }

  /* Various options can be set here... */
  cinfo.dct_method = JDCT_FLOAT;
  // cinfo.dct_method = JDCT_IFAST;

  jpeg_set_quality (&cinfo, priv->adjusted_quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < cinfo.image_height) {
    if (c2 && c3) {
      int n;
      /* Setup necessary for raw downsampled data.  */
      JSAMPROW y[16];
      JSAMPROW cb[16];
      JSAMPROW cr[16];
      for (n=0; n < 16; n++) {
	y[n] = c1 + ((cinfo.next_scanline+n)* width);
	cb[n] = c2 + ((cinfo.next_scanline+n) * width / 2);
	cr[n] = c3 + ((cinfo.next_scanline+n) * width / 2);
      }

      JSAMPARRAY array[3] = { y, cb, cr};
      JSAMPIMAGE image = array;
      /* Need to pass enough lines at a time, see "(num_lines < lines_per_iMCU_row)" test in
	 jcapistd.c */
      jpeg_write_raw_data(&cinfo, image, 16);
    }
    else {
      JSAMPROW row_pointer[1]; /* pointer to a single row */
      row_pointer[0] = c1 + (cinfo.next_scanline *  width * 3);
      jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  }
    
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  if (pi->outputs[OUTPUT_JPEG].destination) {
    PostData(jpeg_out, pi->outputs[OUTPUT_JPEG].destination);
  }
  else {
    /* Discard output buffer! */  
    Jpeg_buffer_discard(jpeg_out);
  }

  jpeg_out = 0L;		/* Clear output buffer copy. */

  /* Calculate compress time. */
  gettimeofday(&t2, 0L);
  float tdiff =  (t2.tv_sec + t2.tv_usec/1000000.0) - (t1.tv_sec + t1.tv_usec/1000000.0);

  if (cfg.verbosity > 0) { 
    if (pi->counter % (30) == 0) {
      printf("%.5f\n", tdiff);
    }
  }

  if ((priv->time_limit > 0.0) && (tdiff > priv->time_limit)) {
    /* Compress time went over time limit, call sched_yield(), which
       should prevent starving other threads, most importantly video
       and audio capture.  Frames will back up on this thread, but on
       "borderline" systems like 1.6GHz P4 it tends to even out. */ 
    sched_yield();

    /* Turn down quality. */
    if (priv->adjusted_quality > 50) {
      priv->adjusted_quality -= 5;
    }

    report_time = 1;
  }
  else if (priv->adjusted_quality < priv->quality) {
    /* Ratchet quality back up, but only by 2, we don't "ping-pong" +/- 5. */
    int temp = priv->adjusted_quality + 2;
    priv->adjusted_quality = _min(temp, priv->quality);
    report_time = 1;
  }

  if (report_time) {
    fprintf(stderr, "* %.5f (q=%d)\n", 
	    tdiff, 
	    priv->adjusted_quality);
  }
}

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void rgb3_handler(Instance *pi, void *data)
{
  RGB3_buffer *rgb3_in = data;
  compress_and_post(pi, 
		    rgb3_in->width, rgb3_in->height,
		    rgb3_in->data, 0L, 0L);
  RGB3_buffer_discard(rgb3_in);
}

static void bgr3_handler(Instance *pi, void *data)
{
  BGR3_buffer *bgr3_in = data;
  RGB3_buffer *rgb3_in = 0L;
  bgr3_to_rgb3(&bgr3_in, &rgb3_in);
  compress_and_post(pi, 
		    rgb3_in->width, rgb3_in->height,
		    rgb3_in->data, 0L, 0L);
  RGB3_buffer_discard(rgb3_in);
}

static void y422p_handler(Instance *pi, void *data)
{
  Y422P_buffer *y422p_in = data;
  compress_and_post(pi, 
		    y422p_in->width, y422p_in->height,
		    y422p_in->y, y422p_in->cb, y422p_in->cr);
  Y422P_buffer_discard(y422p_in);
}


static void CJpeg_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  pi->counter++;
}

static void CJpeg_instance_init(Instance *pi)
{
  CJpeg_private *priv = Mem_calloc(1, sizeof(CJpeg_private));
  priv->quality = 75;
  priv->adjusted_quality = priv->quality;
  priv->time_limit = 0.030;

  pi->data = priv;
}

static Template CJpeg_template = {
  .label = "CJpeg",
  .inputs = CJpeg_inputs,
  .num_inputs = table_size(CJpeg_inputs),
  .outputs = CJpeg_outputs,
  .num_outputs = table_size(CJpeg_outputs),
  .tick = CJpeg_tick,
  .instance_init = CJpeg_instance_init,  
};

void CJpeg_init(void)
{
  Template_register(&CJpeg_template);
}


