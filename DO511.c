#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Images.h"
#include "Cfg.h"
#include "ov511_decomp.h"

enum { INPUT_CONFIG, INPUT_O511 };

static void Config_handler(Instance *pi, void *data);
static void O511_handler(Instance *pi, void *data);

static Input DO511_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_O511 ] = { .type_label = "O511_buffer", .handler = O511_handler },
};

enum { OUTPUT_YUV420P, OUTPUT_RGB3, OUTPUT_GRAY };
static Output DO511_outputs[] = {
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
  [ OUTPUT_RGB3 ] = {.type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_GRAY ] = {.type_label = "GRAY_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  int gspca_mode;
} DO511_private;

static Config config_table[] = {
  { "gspca_mode", 0L, 0L, 0L, cti_set_int, offsetof(DO511_private, gspca_mode) },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


#if 0
static void junk(O511_buffer *o511_in, YUV420P_buffer *y420p_out)
{
  {
    static int x = 0;

    if (++x == 5) {
      x = 1;
      FILE *f;
      f = fopen("/tmp/y.pgm", "wb");
      if (f) {
	fprintf(f, "P5\n%d %d\n255\n",  o511_in->width, o511_in->height);
	if (fwrite(y420p_out->data, o511_in->width * o511_in->height, 1, f) != 1) {
	  perror("fwrite");
	}
	fclose(f);
	
	f = fopen("/tmp/u.pgm", "wb");
	fprintf(f, "P5\n%d %d\n255\n",  o511_in->width/2, o511_in->height/2);
	if (fwrite(y420p_out->data + o511_in->width * o511_in->height, (o511_in->width * o511_in->height)/4, 1, f) != 1) {
	  perror("fwrite");
	}
	fclose(f);
	
	f = fopen("/tmp/v.pgm", "wb");
	fprintf(f, "P5\n%d %d\n255\n",  o511_in->width/2, o511_in->height/2);
	if (fwrite(y420p_out->data + (o511_in->width * o511_in->height)*5/4, (o511_in->width * o511_in->height)/4, 1, f) != 1) {
	  perror("fwrite");
	}
	fclose(f);
      }
    }
  }
}
#endif

/* From libv4l-0.6.4/libv4lconvert/ov511-decomp.c:
   Remove all 0 blocks from input */
static void remove0blocks(unsigned char *pIn, unsigned long *inSize)
{
  long long *in = (long long *)pIn;
  long long *out = (long long *)pIn;
  int i, j;

  for (i = 0; i < *inSize; i += 32, in += 4) {
    int all_zero = 1;
    for (j = 0; j < 4; j++)
      if (in[j]) {
	all_zero = 0;
	break;
      }

    /* Skip 32 byte blocks of all 0 */
    if (all_zero)
      continue;

    for (j = 0; j < 4; j++)
      *out++ = in[j];
  }

  *inSize -= (in - out) * 8;
}

static void O511_handler(Instance *pi, void *data)
{
  DO511_private * priv = (DO511_private *)pi;
  O511_buffer *o511_in = data;
  YUV420P_buffer *y420p_out;
  unsigned char *temp;
  int magic_offset;

  y420p_out = YUV420P_buffer_new(o511_in->width, o511_in->height, 0L);
  y420p_out->c.timestamp = o511_in->c.timestamp;
    
  temp = malloc( (o511_in->width * o511_in->height * 3)/ 2);

  if (priv->gspca_mode) {
    /* I think this was necessary for the GSPCA build. */
    remove0blocks(o511_in->data, &o511_in->data_length);
    magic_offset = 9;
  }
  else {
    /* But this works for the older V4L1 build. */
    magic_offset = 0;
  }

  // printf("o511_in->data_length=%d\n", o511_in->data_length);
  ov511_decomp_420(o511_in->data+magic_offset, 
		   y420p_out->data, 
		   temp, 
		   o511_in->width, 
		   o511_in->height, 
		   o511_in->data_length);
  free(temp);

  if (pi->outputs[OUTPUT_RGB3].destination) {
    RGB3_buffer *rgb = YUV420P_to_RGB3(y420p_out);
    rgb->c.timestamp = o511_in->c.timestamp;
    PostData(rgb, pi->outputs[OUTPUT_RGB3].destination);
  }

  if (pi->outputs[OUTPUT_GRAY].destination) {
    Gray_buffer *gray_out = Gray_buffer_new(y420p_out->width, y420p_out->height, 0L);
    memcpy(gray_out->data, y420p_out->y, y420p_out->width * y420p_out->height);
    gray_out->c.timestamp = o511_in->c.timestamp;
    PostData(gray_out, pi->outputs[OUTPUT_GRAY].destination);
  }

  /* Do this one last.  It will release ownership of the y420p_out buffer. */
  if (pi->outputs[OUTPUT_YUV420P].destination) {
    PostData(y420p_out, pi->outputs[OUTPUT_YUV420P].destination);
    y420p_out = 0L;
  }

  if (y420p_out) {
    YUV420P_buffer_discard(y420p_out); y420p_out = 0L;
  }

  O511_buffer_discard(o511_in);
}


static void DO511_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }
}


static void DO511_instance_init(Instance *pi)
{
  DO511_private * priv = (DO511_private *)pi;
  priv->gspca_mode = 0;
}


static Template DO511_template = {
  .label = "DO511",
  .priv_size = sizeof(DO511_private),
  .inputs = DO511_inputs,
  .num_inputs = table_size(DO511_inputs),
  .outputs = DO511_outputs,
  .num_outputs = table_size(DO511_outputs),
  .tick = DO511_tick,
  .instance_init = DO511_instance_init,
};

void DO511_init(void)
{
  Template_register(&DO511_template);
}
