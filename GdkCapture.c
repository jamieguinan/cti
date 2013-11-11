/*
 * This module polls for the existence of a specific named file,
 * containing GDK image data, reads the file, converts to RGB3 and
 * posts to output, then deletes the file.  It is meant to work with
 * an external program that generates GDK image data and writes the
 * file if/when it disappears.  It all actually works well enough for
 * 30fps video, using /dev/shm or any ramdisk filesystem.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* access */
#include <sys/time.h>		/* gettimeofday */

#include "CTI.h"
#include "GdkCapture.h"
#include "Images.h"
#include "Mem.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input GdkCapture_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

enum { OUTPUT_RGB3 };
static Output GdkCapture_outputs[] = {
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
};

typedef struct {
  Instance i;
  String *filename;
  int seq;
  uint32_t last_checksum;
  uint32_t last_checksum_seq;
  unsigned int idle_quit_threshold;
} GdkCapture_private;


static int set_filename(Instance *pi, const char *value)
{
  GdkCapture_private *priv = (GdkCapture_private *)pi;
  if (priv->filename) {
    String_free(&priv->filename);
  }
  priv->filename = String_new(value);
  return 0;
}


static int set_idle_quit_threshold(Instance *pi, const char *value)
{
  GdkCapture_private *priv = (GdkCapture_private *)pi;
  priv->idle_quit_threshold = atoi(value);
  return 0;
}


static Config config_table[] = {
  { "filename",    set_filename, 0L, 0L },
  { "idle_quit_threshold",   set_idle_quit_threshold, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


void gdk_to_rgb(uint8_t *data, int width, int height, int depth, int bpp, int bpl, RGB3_buffer *rgb)
{
  int x, y;
  uint8_t *src;
  uint8_t *dst;

  if (bpp != 4) {
    return;
  }
  for (y=0; y < height; y++) {
    src = data + (y*bpl);
    dst = rgb->data + (y*rgb->width*3);
    for (x=0; x < width; x++) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      dst += 3;
      src += 4;
    }
  }
}


static void check_files(Instance *pi)
{
  GdkCapture_private *priv = (GdkCapture_private *)pi;
  
  /* Poll for file, because I don't have a good IPC mechanism yet. */
  if (priv->filename == 0L || 
      access(priv->filename->bytes, R_OK) != 0) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/100}, NULL);
    return;
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    char line[256] = {};
    uint8_t *data = 0L;
    RGB3_buffer *rgb = 0L;
    int n;
    int width, height, depth, bpp, bpl;
    int size;
    uint32_t checksum = 0;
    int i;

    FILE *f = fopen(priv->filename->bytes, "rb");
    if (!f) {
      perror(priv->filename->bytes);
      goto out;
    }
    if (fgets(line, sizeof(line), f) == NULL) {
      perror("fgets");
      goto out;
    }
    n = sscanf(line, "%d %d %d %d %d", &width, &height, &depth, &bpp, &bpl);
    if (n != 5) {
      fprintf(stderr, "%s: invalid header\n", __func__);
      goto out;
    }
    rgb = RGB3_buffer_new(width, height, 0L);
    gettimeofday(&rgb->c.tv, 0L);
    size = height*bpl;
    data = Mem_malloc(size);
    if (fread(data, size, 1, f) != 1) {
      fprintf(stderr, "failed reading %d bytes\n", size);
      goto out;
    }
    unlink(priv->filename->bytes);
    for (i=0; i < size; i++) {
      checksum += data[i];
    }
    if (checksum != priv->last_checksum) {
      priv->last_checksum = checksum;
      priv->last_checksum_seq = priv->seq;
    }
    else {
      if (priv->idle_quit_threshold && 
	  (priv->seq - priv->last_checksum_seq) > priv->idle_quit_threshold) {
	printf("quit trigger!\n");
	exit(0);
	priv->idle_quit_threshold = 0;
      }
    }
    gdk_to_rgb(data, width, height, depth, bpp, bpl, rgb);
    Mem_free(data);
    PostData(rgb, pi->outputs[OUTPUT_RGB3].destination);
  out:
    fclose(f);
  }

  unlink("/dev/shm/gdk.cap");
  priv->seq += 1;
}

static void GdkCapture_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  check_files(pi);

  pi->counter++;
}

static void GdkCapture_instance_init(Instance *pi)
{
  // GdkCapture_private *priv = (GdkCapture_private *)pi;
}


static Template GdkCapture_template = {
  .label = "GdkCapture",
  .priv_size = sizeof(GdkCapture_private),
  .inputs = GdkCapture_inputs,
  .num_inputs = table_size(GdkCapture_inputs),
  .outputs = GdkCapture_outputs,
  .num_outputs = table_size(GdkCapture_outputs),
  .tick = GdkCapture_tick,
  .instance_init = GdkCapture_instance_init,
};

void GdkCapture_init(void)
{
  Template_register(&GdkCapture_template);
}
