/* 
 * Transform (rotate/flip) and crop Jpeg buffers, like "jpegtran"
 * command-line app.  Processing order is transform-then-crop.
 * Also adds standard quantization tables if absent.
 */
#include <stdio.h>		
#include <stdlib.h>		/* exit */
#include <string.h>
#include <setjmp.h>

#include <jpeglib.h>
//#include "cdjpeg.h"
#include "transupp.h"

#include "CTI.h"
#include "JpegTran.h"
#include "Images.h"
//#include "jmemsrc.h"
//#include "jmemdst.h"
#include "jpeghufftables.h"

static void Config_handler(Instance *pi, void *data);
static void jpeg_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_JPEG };
static Input JpegTran_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg",  .handler = Config_handler },
  [ INPUT_JPEG ] = { .type_label = "Jpeg_buffer", .handler = jpeg_handler },
};

enum { OUTPUT_JPEG };
static Output JpegTran_outputs[] = {
  [ OUTPUT_JPEG ] = { .type_label = "Jpeg_buffer", .destination = 0L },
};

typedef struct  {
  Instance i;
  jpeg_transform_info info;
} JpegTran_private;

static struct {
  const char *enumString;
  int enumValue;
} transform_map[] = {
  { "NONE", JXFORM_NONE },
  { "FLIP_H", JXFORM_FLIP_H },
  { "FLIP_V", JXFORM_FLIP_V },
  { "TRANSPOSE", JXFORM_TRANSPOSE },
  { "TRANSVERSE", JXFORM_TRANSVERSE },
  { "ROT_90", JXFORM_ROT_90 },
  { "ROT_180", JXFORM_ROT_180 },
  { "ROT_270", JXFORM_ROT_270 },
};

static int set_transform(Instance *pi, const char *value)
{
  JpegTran_private *priv = (JpegTran_private *)pi;
  int i;

  for (i=0; i < table_size(transform_map); i++) {
    if (streq(transform_map[i].enumString, value)) {
      priv->info.transform = transform_map[i].enumValue;
      printf("transform set to %s\n", value);
      return 0;
    }
  }
  
  return 1;
}


static int set_crop(Instance *pi, const char *value)
{
  JpegTran_private *priv = (JpegTran_private *)pi;
  int rc;

  rc = jtransform_parse_crop_spec(&priv->info, value);

  if (rc == TRUE) {
    return 0;
  }
  else {
    return 1;
  }
}

static Config config_table[] = {
  { "transform",  set_transform, 0L, 0L},
  { "crop", set_crop, 0L, 0L},	/* Use jtransform_parse_crop_spec */
};


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


static void transform(JpegTran_private *priv, Jpeg_buffer *jpeg_in, Jpeg_buffer **out)
{
  /* This is a mostly borrowed from "jpeg-7/jpegtran.c", with parts from "CJpeg.c" and "DJpeg.c". */
  int error = 0;
  jmp_buf jb;
  Jpeg_buffer *jpeg_out = 0L;
  struct jpeg_decompress_struct srcinfo;
  int srcinfo_needs_cleanup = 0;
  struct jpeg_compress_struct dstinfo;
  int dstinfo_needs_cleanup = 0;
  struct jpeg_error_mgr jsrcerr, jdsterr;
  jvirt_barray_ptr * src_coef_arrays;
  jvirt_barray_ptr * dst_coef_arrays;

  srcinfo.err = jpeg_std_error(&jsrcerr);

  srcinfo.err->emit_message = jerr_warning_noop;
  srcinfo.err->error_exit = jerr_error_handler;    

  jpeg_create_decompress(&srcinfo);
  srcinfo_needs_cleanup = 1;

  srcinfo.client_data = &jb;
  error = setjmp(jb);
  if (error) {
    goto out_check_errors;
  }

  dstinfo.err = jpeg_std_error(&jdsterr);

  jpeg_create_compress(&dstinfo);
  dstinfo_needs_cleanup = 1;

  srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

  jpeg_mem_src(&srcinfo, jpeg_in->data, jpeg_in->data_length);

  /* Enable saving of extra markers that we want to copy */
  jcopy_markers_setup(&srcinfo, JCOPYOPT_ALL);

  /* Read file header */
  (void) jpeg_read_header(&srcinfo, TRUE);

  /* NOTE: What does this do with uninitizlied info? */
  jtransform_request_workspace(&srcinfo, &priv->info);

  /* Set quantization tables if not already set.  For example, the
     Logitech Quickcam Pro 9000 models produce Jpegs lacking these
     tables. */
  if (!srcinfo.dc_huff_tbl_ptrs[0]) {
    // fprintf(stderr, "%s: adding dc huff tables", __func__);
    srcinfo.dc_huff_tbl_ptrs[0] = dc_huff_tbl_ptrs[0];
    srcinfo.dc_huff_tbl_ptrs[1] = dc_huff_tbl_ptrs[1];
  }
  
  if (!srcinfo.ac_huff_tbl_ptrs[0]) {
    srcinfo.ac_huff_tbl_ptrs[0] = ac_huff_tbl_ptrs[0];
    srcinfo.ac_huff_tbl_ptrs[1] = ac_huff_tbl_ptrs[1];
  }

  /* Read source file as DCT coefficients.  Note, this may trigger
     the longjmp error handling.  */
  src_coef_arrays = jpeg_read_coefficients(&srcinfo);

  /* Initialize destination compression parameters from source values */
  jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

  // fprintf(stderr, "dstinfo.jpeg_color_space=%d\n", dstinfo.jpeg_color_space);

  dst_coef_arrays = jtransform_adjust_parameters(&srcinfo, &dstinfo,
						 src_coef_arrays,
						 &priv->info);

  /* Specify data destination for compression */

  /*  Leave enough space for 100% of original size, plus some header. */
  jpeg_out = Jpeg_buffer_new(srcinfo.image_width*srcinfo.image_height*3+16384, &jpeg_in->c);

  //jpeg_out->c.tv.tv_sec = 0L;
  //jpeg_out->c.tv.tv_usec = 0L;

  jpeg_mem_dest(&dstinfo, &jpeg_out->data, &jpeg_out->encoded_length);

  /* Start compressor (note no image data is actually written here) */
  jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

  /* Copy to the output file any extra markers that we want to preserve */
  jcopy_markers_execute(&srcinfo, &dstinfo, JCOPYOPT_ALL);

  /* Execute image transformation, if any */
  jtransform_execute_transformation(&srcinfo, &dstinfo,
				    src_coef_arrays,
				    &priv->info);

 out_check_errors:
  if (!error) {
    jpeg_finish_compress(&dstinfo);
    (void) jpeg_finish_decompress(&srcinfo);
  }

  if (dstinfo_needs_cleanup) {
    jpeg_destroy_compress(&dstinfo);
  }

  if (srcinfo_needs_cleanup) {
    jpeg_destroy_decompress(&srcinfo);
  }

  if (jpeg_out) {
    if (error) {
      Jpeg_buffer_release(jpeg_out);
      /* Caller should checl for result being NULL. */
      jpeg_out = 0L;
    }
    else {
      jpeg_out->width = dstinfo.image_width;
      jpeg_out->height = dstinfo.image_height;
    }
  }

  *out = jpeg_out;
}

static void jpeg_handler(Instance *pi, void *msg)
{
  JpegTran_private *priv = (JpegTran_private *)pi;
  Jpeg_buffer *jpeg_in = msg;

  //fprintf(stderr, "%s: got Jpeg_buffer %p dest %p\n", __func__, jpeg_in,
  //    pi->outputs[OUTPUT_JPEG].destination);

  if (pi->outputs[OUTPUT_JPEG].destination) {
    Jpeg_buffer * jpeg_out = 0L;
    // fprintf(stderr, "transform...\n");
    transform(priv, jpeg_in, &jpeg_out);
    if (jpeg_out) {
      PostData(jpeg_out, pi->outputs[OUTPUT_JPEG].destination);
    }
  }
  
  Jpeg_buffer_release(jpeg_in);
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void JpegTran_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}


static void JpegTran_instance_init(Instance *pi)
{
  JpegTran_private *priv = (JpegTran_private *)pi;

  /* From jpegtran.c:parse_switches() */
  priv->info.transform = JXFORM_NONE;
  priv->info.trim = FALSE;
  priv->info.perfect = FALSE;
  priv->info.force_grayscale = FALSE;
  priv->info.crop = FALSE;

  
}

static Template JpegTran_template = {
  .label = "JpegTran",
  .priv_size = sizeof(JpegTran_private),
  .inputs = JpegTran_inputs,
  .num_inputs = table_size(JpegTran_inputs),
  .outputs = JpegTran_outputs,
  .num_outputs = table_size(JpegTran_outputs),
  .tick = JpegTran_tick,
  .instance_init = JpegTran_instance_init,
};

void JpegTran_init(void)
{
  Template_register(&JpegTran_template);
}
