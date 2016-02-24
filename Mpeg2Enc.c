#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include "CTI.h"
#include "String.h"
#include "Images.h"
#include "Mem.h"

static void Config_handler(Instance *pi, void *msg);
static void Y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_YUV422P };
static Input Mpeg2Enc_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = Y422p_handler },
};

enum { OUTPUT_FEEDBACK };
static Output Mpeg2Enc_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  String *vout;
  FILE *po;			/* File or pipe output... */
  int header_sent;
} Mpeg2Enc_private;


static int set_vout(Instance *pi, const char *value)
{
  Mpeg2Enc_private *priv = (Mpeg2Enc_private *)pi;

  if (priv->vout) {
    String_free(&priv->vout);
  }

  if (priv->po) {
    pclose(priv->po);
  }

  priv->vout = String_new(value);

  char path[256+strlen(priv->vout->bytes)+1];
  /* Need to pass through y4mscaler to go from 4:2:2 to 4:2:0, which is the only format mpeg2enc accepts. */
  /* "-O preset=DVD", needed?  No, bad, it fills in to 720wide.*/
  sprintf(path, "y4mscaler -v 0 -O chromass=420MPEG2 -I sar=NTSC | mpeg2enc -I 0 -M 2 -v 0 -f 8 -b 7000 -o %s", priv->vout->bytes);

  //sprintf(path, "y4mscaler -v 0 -O chromass=420MPEG2 -I sar=NTSC | y4mdenoise -t 4 | mpeg2enc -M 2 -v 0 -f 8 -b 7000 -o %s", priv->vout->bytes);
  //sprintf(path, "y4mscaler -v 0 -O chromass=420MPEG2 | y4mdenoise | mpeg2enc -M 1 -v 0 -f 8 -o %s", priv->vout->bytes);
  //sprintf(path, "y4mscaler -v 0 -O chromass=420MPEG2 | yuvdenoise | mpeg2enc -M 1 -v 0 -f 8 -o %s", priv->vout->bytes);
  // sprintf(path, "ffmpeg -i /dev/stdin -target dvd -b 3500k -y %s", priv->vout->bytes);
  //sprintf(path, "cat > out.y4m");
  priv->po = popen(path, "w");

  return 0;
}

static int set_mode(Instance *pi, const char *value)
{
  // Mpeg2Enc_private *priv = (Mpeg2Enc_private *)pi;
  /* Set either mpeg2enc or ffmpeg mode. */
  return 0;
}


static Config config_table[] = {
  { "vout", set_vout, 0L, 0L},
  { "mode", set_mode, 0L, 0L},
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Y422p_handler(Instance *pi, void *msg)
{
  Mpeg2Enc_private *priv = (Mpeg2Enc_private *)pi;
  YUV422P_buffer *y422p_in = msg;
  int n = 0;

#if 0    
  if (y422p_in == 0L) {
    /* End of stream. */
    pclose(priv->po);
    priv->po = 0L;
    return;
  }
#endif

  // printf("%s got a frame %p\n", __func__, y422p_in);
  
  if (!priv->po) {
    fprintf(stderr, "%s: pipe output not set!\n", __func__);
    goto out;
  }
    
  if (!priv->header_sent) {
    /* 
     * Create and write header.
     *     It: interlaced, top-field first
     *   C422: 4:2:2 chroma subsampling
     */
    fprintf(priv->po, "YUV4MPEG2 W%d H%d F30000:1001 A10:11 It C422\n",
	    y422p_in->width,
	    y422p_in->height
	    );
    priv->header_sent = 1;
  }
  
  /* Create and write one frame. */
  fprintf(priv->po, "FRAME\n");
  
  n += fwrite(y422p_in->y, y422p_in->y_length, 1, priv->po);
  n += fwrite(y422p_in->cb, y422p_in->cb_length, 1, priv->po);
  n += fwrite(y422p_in->cr, y422p_in->cr_length, 1, priv->po);

  if (n != 3) {
    fprintf(stderr, "%s: write error\n", __func__);
    pclose(priv->po);
    priv->po = NULL;
    goto out;
  }

  if (pi->outputs[OUTPUT_FEEDBACK].destination) {
    Feedback_buffer *fb = Feedback_buffer_new();
    /* FIXME:  Maybe could get propagate sequence and pass it back here... */
    fb->seq = 0;
    PostData(fb, pi->outputs[OUTPUT_FEEDBACK].destination);
  }
  
 out:
  YUV422P_buffer_release(y422p_in);
}


static void Mpeg2Enc_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Mpeg2Enc_instance_init(Instance *pi)
{
  // Mpeg2Enc_private *priv = (Mpeg2Enc_private *)pi;
}


static Template Mpeg2Enc_template = {
  .label = "Mpeg2Enc",
  .priv_size = sizeof(Mpeg2Enc_private),
  .inputs = Mpeg2Enc_inputs,
  .num_inputs = table_size(Mpeg2Enc_inputs),
  .outputs = Mpeg2Enc_outputs,
  .num_outputs = table_size(Mpeg2Enc_outputs),
  .tick = Mpeg2Enc_tick,
  .instance_init = Mpeg2Enc_instance_init,
};

void Mpeg2Enc_init(void)
{
  Template_register(&Mpeg2Enc_template);
}
