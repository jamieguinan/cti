/* Search and replace "Y4MOverlay" with new module name. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Y4MOverlay.h"
#include "Images.h"
#include "Array.h"
#include "File.h"

static void Config_handler(Instance *pi, void *msg);
static void y420p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV420P };
static Input Y4MOverlay_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = y420p_handler },
};

enum { OUTPUT_YUV420P };
static Output Y4MOverlay_outputs[] = {
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  YUV420P_buffer * img;
} Y4MOverlay_private;


static void parse_y4m_header(ArrayU8 *data, uint8_t ** framestart, int * pwidth, int * pheight)
{
  /* NOTE: This was copied from Y4MInput.c.  It would be nice to merge this, Y4MInput.c,
     Images.c, and ImageLoader.c to share code. */
  {
    int eoh = -1;
    int soh = ArrayU8_search(data, 0, ArrayU8_temp_const("YUV4MPEG2", 9));
    if (soh == 0)  {
      /* Search for newline at end of header line. */
      eoh = ArrayU8_search(data, soh, ArrayU8_temp_const("\n", 1));
    }

    if (soh != 0 || eoh == -1) {
      fprintf(stderr, "could lot find YUV4MPEG2 header\n");
      return;
    }

    /* Parse header line. */
    String *s = ArrayU8_extract_string(data, soh, eoh);
    String_list *ls = String_split(s, " ");
    int i;
    for (i=0; i < String_list_len(ls); i++) {
      String *t = String_list_get(ls, i);
      if (String_is_none(t)) {
	continue;
      }

      int width;
      //int width_set = 0;
      int height;
      //int height_set = 0;
      int frame_numerator;
      //int frame_numerator_set = 0;
      int frame_denominator;
      //int frame_denominator_set = 0;
      int aspect_numerator;
      //int aspect_numerator_set = 0;
      int aspect_denominator;
      //int aspect_denominator_set = 0;
      char chroma_subsamping[32+1];
      //int chroma_subsamping_set = 0;
      char interlacing;
      //int interlacing_set = 0;

      if (sscanf(t->bytes, "W%d", &width) == 1) {
	printf("width %d\n", width);
	*pwidth = width;
      }
      else if (sscanf(t->bytes, "H%d", &height) == 1) {
	printf("height %d\n", height);
	*pheight = height;
      }
      else if (sscanf(t->bytes, "C%32s", chroma_subsamping) == 1) {
	printf("chroma subsampling %s\n", chroma_subsamping);
      }
      else if (sscanf(t->bytes, "I%c", &interlacing) == 1) {
	printf("interlacing I%c\n", interlacing);
	//switch (interlacing) {
	//case 't': priv->video_common.interlace_mode = IMAGE_INTERLACE_TOP_FIRST; break;
	//case 'b': priv->video_common.interlace_mode = IMAGE_INTERLACE_BOTTOM_FIRST; break;
	//case 'm': priv->video_common.interlace_mode = IMAGE_INTERLACE_MIXEDMODE; break;
	//case 'p': 
	//default:
	//priv->video_common.interlace_mode = IMAGE_INTERLACE_NONE; break;
	//}
      }
      else if (sscanf(t->bytes, "F%d:%d", &frame_numerator, &frame_denominator) == 2) {
	//priv->video_common.nominal_period = (1.0*frame_denominator/frame_numerator);
	//priv->video_common.fps_numerator = frame_numerator;
	//priv->video_common.fps_denominator = frame_denominator;
	//printf("frame rate %d:%d (%.5f period)\n", 
	//priv->video_common.fps_numerator,
	//priv->video_common.fps_denominator,
	//priv->video_common.nominal_period);
      }
      else if (sscanf(t->bytes, "A%d:%d", &aspect_numerator, &aspect_denominator) == 2) {
	printf("aspect %d:%d\n", aspect_numerator, aspect_denominator);
      }
      // else if (sscanf(t->bytes, "X%s", metadata) == 1) { }
    }

    String_list_free(&ls);
    
    //priv->state = PARSING_FRAME;
    /* Trim out header plus newline. */
    // ArrayU8_trim_left(data, eoh+1);
    *framestart = data->data + eoh + 1 + strlen("FRAME\n");
  } 
}

static int set_input(Instance *pi, const char *value)
{
  Y4MOverlay_private * priv = (Y4MOverlay_private *)pi;
  ArrayU8 * fdata = File_load_data(S((char*)value));
  if (!fdata) {
    return 1;
  }

  int width = 0, height = 0;
  uint8_t * framestart = NULL;
  parse_y4m_header(fdata, &framestart, &width, &height);

  priv->img = YUV420P_buffer_from(framestart, width, height, NULL);

  printf("priv->img=%p\n", priv->img);
  return 0;
}

static Config config_table[] = {
  { "input",    set_input, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void y420p_handler(Instance *pi, void *msg)
{
  Y4MOverlay_private * priv = (Y4MOverlay_private *)pi;
  YUV420P_buffer * buf = msg;

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    /* Compose overlay, pass along. */
    if (priv->img 
	&& priv->img->width == buf->width
	&& priv->img->height == buf->height) {
      int y, x;

      uint8_t * py = priv->img->y;
      uint8_t * pcb = priv->img->cb;
      uint8_t * pcr = priv->img->cr;

      uint8_t * by = buf->y;
      uint8_t * bcb = buf->cb;
      uint8_t * bcr = buf->cr;

      for (y=0; y < buf->height; y++) {
	for (x=0; x < buf->width; x++) {
	  if ( 1/* *py != 0x10 */ ) {
	    *by = *py;
	    *bcb = *pcb;
	    *bcr = *pcr;
	  }
	  py++;
	  by++;
	  if (x%4 == 0) {
	    pcb++;
	    pcr++;
	    bcb++;
	    bcr++;
	  }
	}
      }
    }
    PostData(buf, pi->outputs[OUTPUT_YUV420P].destination);
  }
  else {
    YUV420P_buffer_discard(buf);
  }
}


static void Y4MOverlay_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Y4MOverlay_instance_init(Instance *pi)
{
  // Y4MOverlay_private *priv = (Y4MOverlay_private *)pi;
}


static Template Y4MOverlay_template = {
  .label = "Y4MOverlay",
  .priv_size = sizeof(Y4MOverlay_private),  
  .inputs = Y4MOverlay_inputs,
  .num_inputs = table_size(Y4MOverlay_inputs),
  .outputs = Y4MOverlay_outputs,
  .num_outputs = table_size(Y4MOverlay_outputs),
  .tick = Y4MOverlay_tick,
  .instance_init = Y4MOverlay_instance_init,
};

void Y4MOverlay_init(void)
{
  Template_register(&Y4MOverlay_template);
}
