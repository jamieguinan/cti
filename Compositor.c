/*
 * Compose input images into output images.
 */

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "CTI.h"
#include "Compositor.h"
#include "Images.h"

#define RED "[31m"
#define CLEAR "[0m"

static void Config_handler(Instance *pi, void *msg);
static void YUV420P_handler(Instance *pi, void *msg);

static int set_size(Instance *pi, const char *value);
static int set_require(Instance *pi, const char *value);
static int set_paste(Instance *pi, const char *value);
static int do_clear(Instance *pi, const char *value);

enum { INPUT_CONFIG, INPUT_YUV420P };
static Input Compositor_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
};

enum { OUTPUT_YUV420P };
static Output Compositor_outputs[] = {
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
};

typedef struct {
  String * label;
  struct {
    int x, y, w, h;
  } src;
  struct {
    int x, y;
  } dest;
  int rotation;
} PasteOp;


typedef struct {
  Instance i;
  struct {
    int width;
    int height;
  } size;
  String_list * required_labels;
  Array(PasteOp *) operations;
  Array(YUV420P_buffer *) yuv420p_images;
} Compositor_private;

static Config config_table[] = {
  { "size",    set_size, NULL, NULL },
  { "require", set_require, NULL, NULL },
  { "paste",   set_paste, NULL, NULL },
  { "clear",   do_clear, NULL, NULL },
};


static int set_size(Instance *pi, const char *value)
{
  Compositor_private *priv = (Compositor_private *)pi;
  int rc = 0;
  int n = sscanf(value, "%dx%d"
		 , &priv->size.width
		 , &priv->size.height
		 );
  if (n != 2) {
    fprintf(stderr, "bad size specifier\n");
    rc = 1;
  }

  return rc;
}

static int set_require(Instance *pi, const char *value)
{
  Compositor_private *priv = (Compositor_private *)pi;
  int rc = 0;
  String_list * labels = String_split_s(value, ",");
  int i;
  for (i=0; i < String_list_len(labels); i++) {
    char * label = s(String_list_get(labels, i));
    char dest[64];
    if (sscanf(label, "%64[A-Za-z0-9]", dest) != 1) {
      fprintf(stderr, "bad label %s\n", s(String_list_get(labels, i)));
      rc = 1;
      break;
    }
    else {
      fprintf(stderr, "label %s Ok\n", label);
    }
  }
  if (rc == 0) {
    if (priv->required_labels) {
      String_list_free(&priv->required_labels);
    }
    priv->required_labels = labels;
    fprintf(stderr, "%d labels\n", String_list_len(labels));
  }
  return rc;
}

static int set_paste(Instance *pi, const char *value)
{
  Compositor_private *priv = (Compositor_private *)pi;
  PasteOp * p = Mem_calloc(1, sizeof(*p));
  int rc = 0;
  char * label = NULL;
  int n = sscanf(value, "%m[A-Za-z0-9]:%d,%d,%d,%d:%d,%d:%d"
		 , &label
		 , &p->src.x
		 , &p->src.y
		 , &p->src.w
		 , &p->src.h
		 , &p->dest.x
		 , &p->dest.y
		 , &p->rotation
		 );
  if ((n != 8)
      || (p->src.x % 2 != 0)
      || (p->src.y % 2 != 0)
      || (p->src.w % 2 != 0)
      || (p->src.h % 2 != 0)
      || (p->dest.x % 2 != 0)
      || (p->dest.y % 2 != 0)
      || (p->rotation % 90 != 0)
      ) {
    fprintf(stderr, "%s:%s bad paste specification\n", __FILE__, __func__);
    free(p);
    rc = 1;
  }
  else {
    p->label = String_new(label);
    p->rotation = (p->rotation + 360) % 360;
    Array_append(priv->operations, p);
  }

  if (label) { free(label); }  
  return rc;
}

static int do_clear(Instance *pi, const char *value)
{
  Compositor_private *priv = (Compositor_private *)pi;

  while (priv->operations.count) {
    PasteOp * p = Array_get(priv->operations, 0);
    Array_delete(priv->operations, 0);
    String_clear(&p->label);
    Mem_free(p);
  }
  return 0;
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

#define PLANE_PASTE_DEBUG 0

static void plane_paste(uint8_t * src, int src_w, int src_h,
			uint8_t * dest, int dest_w, int dest_h,
			PasteOp * p)
{
  /* Handles cases where source rect exceeds boundaries of source image.
     Copy and rotate are done in a single operation. */

#if PLANE_PASTE_DEBUG  
  fprintf(stderr, "%s:%s pasteOp@%p src={%d,%d %d,%d} dest={%d,%d} rotation=%d\n"
	  , __FILE__
	  , __func__
	  , p
	  , p->src.x
	  , p->src.y
	  , p->src.w
	  , p->src.h
	  , p->dest.x
	  , p->dest.y
	  , p->rotation);
#endif
  uint8_t * psrc=NULL;
  uint8_t * pdest=NULL;
  PasteOp op = *p;  /* Use a local copy of the operation, so it can be modified. */
  int x, y;
  int x_count, y_count;
  const int d_src_x=1;	/* same for all rotations */
  const int d_src_y = (src_w);
  int d_dest_x=0, d_dest_y=0;
  

  /* If op source rectangle begins right or below source image. */  
  if (op.src.x >= src_w) { return; }
  if (op.src.y >= src_h) { return; }

  /* If op source rectangle begins left or above source image. */
  if (op.src.x < 0) { op.src.w = cti_max(0, op.src.w + op.src.x); op.src.x = 0; }
  if (op.src.y < 0) { op.src.h = cti_max(0, op.src.h + op.src.y); op.src.y = 0; }

  if (op.src.w <= 0) { return; }
  if (op.src.h <= 0) { return; }

  x_count = op.src.w - cti_max(0, op.src.x + op.src.w - src_w);
  y_count = op.src.h - cti_max(0, op.src.y + op.src.h - src_h);

  psrc = src;
  psrc += op.src.x;
  psrc += (op.src.y * src_w);

  /* At this point we are done with op.src, having used it to calculate psrc, x_count, y_count. */

  if (op.rotation == 0) {
    if (op.dest.x >= dest_w || op.dest.y >= dest_h) { return; }
    if (op.dest.x < 0) { psrc += -(op.dest.x); x_count -= -(op.dest.x); ; op.dest.x = 0;}
    if (op.dest.y < 0) { psrc += (src_w * -(op.dest.y)); y_count -= -(op.dest.y); op.dest.y = 0;}
    if (op.dest.x + x_count > dest_w) { x_count -= (op.dest.x + x_count - dest_w); }
    if (op.dest.y + y_count > dest_h) { y_count -= (op.dest.y + y_count - dest_h); }
    d_dest_x = 1;
    d_dest_y = (dest_w);
    pdest = dest + op.dest.x + (dest_w * (op.dest.y)); /* no dest pixel change */
  }
  else if (op.rotation == 90) {
    if (op.dest.x <= 0 || op.dest.y >= dest_h) { return; }
    if (op.dest.x > dest_w) { psrc += (src_w * (op.dest.x - dest_w)); y_count -= (op.dest.x - dest_w); op.dest.x = dest_w; }
    if (op.dest.y < 0) { psrc += (-op.dest.y); x_count += (op.dest.y); op.dest.y = 0; }
    if (op.dest.x - y_count < 0) { y_count = op.dest.x; }
    if (op.dest.y + x_count > dest_h) { x_count = dest_h - op.dest.y; }
    d_dest_x = (dest_w);
    d_dest_y = -1;
    pdest = dest + (op.dest.x-1) + (dest_w * (op.dest.y)); /* dest pixel left 1 because of rotation */
  }
  else if (op.rotation == 180) {
    if (op.dest.x <= 0 || op.dest.y <= 0) { return; }
    if (op.dest.x > dest_w) { psrc += (op.dest.x - dest_w); x_count -= (op.dest.x - dest_w) ; op.dest.x = dest_w; }
    if (op.dest.y > dest_h) { psrc += (src_w * (op.dest.y - dest_h)); y_count -= (op.dest.y - dest_h); op.dest.y = dest_h; }
    if (op.dest.x - x_count < 0) { x_count = op.dest.x; }
    if (op.dest.y - y_count < 0) { y_count = op.dest.y; }
    d_dest_x = -1;
    d_dest_y = (-dest_w);
    pdest = dest + (op.dest.x-1) + (dest_w * (op.dest.y-1)); /* dest pixel up and left 1 because of rotation */
  }
  else if (op.rotation == 270) {
    if (op.dest.x > dest_w || op.dest.y < 0) { return; }
    if (op.dest.x < 0) { psrc += (src_w * -(op.dest.x)); y_count += op.dest.x; op.dest.x = 0; }
    if (op.dest.y > dest_h) { psrc += (op.dest.y - dest_h); x_count -= (op.dest.y - dest_h); op.dest.y = dest_h; }
    if (op.dest.x + y_count > dest_w) { y_count = (op.dest.x + y_count - dest_w); }
    if (op.dest.y - x_count < 0) { x_count = op.dest.y; }
    d_dest_x = -(dest_w);
    d_dest_y = 1;
    pdest = dest + op.dest.x + (dest_w * (op.dest.y-1)); /* dest pixel up 1 because of rotation */
  }
  else {
    fprintf(stderr, "invalid rotation %d\n", op.rotation);
    return;
  }

#if PLANE_PASTE_DEBUG
  fprintf(stderr, "%s:%s.%d x_count=%d y_count=%d\n", __FILE__, __func__, __LINE__, x_count, y_count);
  fprintf(stdrer, "starting at %ld -> %ld\n", (psrc-src), (pdest-dest));
#endif

  /* Having done all the calculations above, the copy is done in
     a nested for loop. */
  for (y = 0; y < y_count; y += 1) {
    /* Note, could do a memcpy if (d_src_x == d_dest_x == 1) */
    for (x = 0; x < x_count; x += 1) {
#if PLANE_PASTE_DEBUG      
      if (psrc < src) { fprintf(stderr, RED "psrc underflow at %d,%d" CLEAR "\n", x, y); return;}
      if (psrc >= src+(src_w*src_h)) { fprintf(stderr, RED "psrc overflow at %d,%d" CLEAR "\n", x, y); return;} 
      if (pdest < dest) { fprintf(stderr, RED "pdest underflow at %d,%d" CLEAR "\n", x, y); return;}
      if (pdest >= dest+(dest_w*dest_h)) { fprintf(stderr, RED "pdest overflow at %d,%d" CLEAR "\n", x, y); return;}
#endif
      *pdest = *psrc;
      pdest += d_dest_x;
      psrc += d_src_x;
    }
    pdest += (d_dest_y-(x_count*d_dest_x));
    psrc += (d_src_y-x_count);
  }
}


static void yuv420_paste(YUV420P_buffer * src, YUV420P_buffer * dest, PasteOp * p)
{
  plane_paste(src->y, src->width, src->height, dest->y, dest->width, dest->height, p);
  PasteOp chromaOp = *p;
  chromaOp.src.x /= 2;
  chromaOp.src.y /= 2;
  chromaOp.src.w /= 2;
  chromaOp.src.h /= 2;
  chromaOp.dest.x /= 2;
  chromaOp.dest.y /= 2;
  plane_paste(src->cb, src->cb_width, src->cb_height, dest->cb, dest->cb_width, dest->cb_height, &chromaOp);
  plane_paste(src->cr, src->cr_width, src->cr_height, dest->cr, dest->cr_width, dest->cr_height, &chromaOp);
}

static void check_list(Instance *pi)
{
  /* Walk the list of images and see if have one of each label. If so, apply the stored 
     operations and post the output. */
  Compositor_private *priv = (Compositor_private *)pi;

  int labelIndex, imageIndex, opIndex;
  int labelsFound = 0;
  for (labelIndex = 0; labelIndex < String_list_len(priv->required_labels); labelIndex++) {
    for (imageIndex = 0; imageIndex < Array_count(priv->yuv420p_images); imageIndex++) {
      YUV420P_buffer * img = Array_get(priv->yuv420p_images, imageIndex);
      if (String_eq(img->c.label, String_list_get(priv->required_labels, labelIndex))) {
	labelsFound += 1;
	break;
      }
    }
    if (imageIndex == Array_count(priv->yuv420p_images)) {
      break;
    }
  }

  if (labelsFound != String_list_len(priv->required_labels)) {
    // fprintf(stderr, "%s:%s did not find all required labels\n", __FILE__, __func__);
    return;
  }

  // fprintf(stderr, "%s:%s found %d labels\n", __FILE__, __func__, labelsFound);

  YUV420P_buffer * outimg = YUV420P_buffer_new(priv->size.width, priv->size.height, NULL);

  for (opIndex = 0; opIndex < Array_count(priv->operations); opIndex++) {
    /* FIXME: Maybe walk the image list backwards to get most recent if multiple 
       images with same label. */
    PasteOp * p = Array_get(priv->operations, opIndex);
    for (imageIndex = 0; imageIndex < Array_count(priv->yuv420p_images); imageIndex++) {
      YUV420P_buffer * img = Array_get(priv->yuv420p_images, imageIndex);
      if (String_eq(p->label, img->c.label)) {
	/* Apply operation, remove image from list, release image. */
	yuv420_paste(img, outimg, p);
	break;
      }
    }
  }

  /* Clear all images. If one source produces faster than
     another, they could back up. */
  while (Array_count(priv->yuv420p_images)) {
      YUV420P_buffer * img = Array_get(priv->yuv420p_images, 0);
      Array_delete(priv->yuv420p_images, 0);
      YUV420P_buffer_release(img);
  }

  if (!pi->outputs[OUTPUT_YUV420P].destination) {
    fprintf(stderr, "%s:%s caller did not check destination\n", __FILE__, __func__);
    exit(1);
  }
  
  PostData(outimg, pi->outputs[OUTPUT_YUV420P].destination);
}

static void YUV420P_handler(Instance *pi, void *msg)
{
  Compositor_private *priv = (Compositor_private *)pi;
  YUV420P_buffer * img = msg;
  if (!pi->outputs[OUTPUT_YUV420P].destination) {
    fprintf(stderr, "no output configured, discarding input image\n");
    YUV420P_buffer_release(img);
  }
  else {
    Array_append(priv->yuv420p_images, img);
    check_list(pi);
  }
}

static void Compositor_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void Compositor_instance_init(Instance *pi)
{
  // Compositor_private *priv = (Compositor_private *)pi;
}


static Template Compositor_template = {
  .label = "Compositor",
  .priv_size = sizeof(Compositor_private),  
  .inputs = Compositor_inputs,
  .num_inputs = table_size(Compositor_inputs),
  .outputs = Compositor_outputs,
  .num_outputs = table_size(Compositor_outputs),
  .tick = Compositor_tick,
  .instance_init = Compositor_instance_init,
};

void Compositor_init(void)
{
  Template_register(&Compositor_template);
}

