/*
 * SDL video output, keyboard input.
 */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>
#include <math.h>		/* fabs */
#include <SDL.h>
#include <gl.h>
// #include <glu.h>

#include "CTI.h"
#include "Images.h"
#include "Cfg.h"
#include "Keycodes.h"
#include "Pointer.h"


static int SDLtoCTI_Keymap[SDLK_LAST] = {
  [SDLK_UP] = CTI__KEY_UP, 
  [SDLK_DOWN] = CTI__KEY_DOWN, 
  [SDLK_LEFT] = CTI__KEY_LEFT, 
  [SDLK_RIGHT] = CTI__KEY_RIGHT,
  [SDLK_0] = CTI__KEY_0,
  [SDLK_1] = CTI__KEY_1,
  [SDLK_2] = CTI__KEY_2,
  [SDLK_3] = CTI__KEY_3,
  [SDLK_4] = CTI__KEY_4,
  [SDLK_5] = CTI__KEY_5,
  [SDLK_6] = CTI__KEY_6,
  [SDLK_7] = CTI__KEY_7,
  [SDLK_8] = CTI__KEY_8,
  [SDLK_9] = CTI__KEY_9,
  [SDLK_a] = CTI__KEY_A,
  [SDLK_b] = CTI__KEY_B,
  [SDLK_c] = CTI__KEY_C,
  [SDLK_d] = CTI__KEY_D,
  [SDLK_e] = CTI__KEY_E,
  [SDLK_f] = CTI__KEY_F,
  [SDLK_g] = CTI__KEY_G,
  [SDLK_h] = CTI__KEY_H,
  [SDLK_i] = CTI__KEY_I,
  [SDLK_j] = CTI__KEY_J,
  [SDLK_k] = CTI__KEY_K,
  [SDLK_l] = CTI__KEY_L,
  [SDLK_m] = CTI__KEY_M,
  [SDLK_n] = CTI__KEY_N,
  [SDLK_o] = CTI__KEY_O,
  [SDLK_p] = CTI__KEY_P,
  [SDLK_q] = CTI__KEY_Q,
  [SDLK_r] = CTI__KEY_R,
  [SDLK_s] = CTI__KEY_S,
  [SDLK_t] = CTI__KEY_T,
  [SDLK_u] = CTI__KEY_U,
  [SDLK_v] = CTI__KEY_V,
  [SDLK_w] = CTI__KEY_W,
  [SDLK_x] = CTI__KEY_X,
  [SDLK_y] = CTI__KEY_Y,
  [SDLK_z] = CTI__KEY_Z,
};


static void Config_handler(Instance *pi, void *data);
static void Y422P_handler(Instance *pi, void *data);
static void RGB3_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);
static void GRAY_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_422P, INPUT_RGB3, INPUT_BGR3, INPUT_GRAY, INPUT_KEYCODE };
static Input SDLstuff_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
  [ INPUT_GRAY ] = { .type_label = "GRAY_buffer", .handler = GRAY_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_FEEDBACK, OUTPUT_CONFIG, OUTPUT_KEYCODE, OUTPUT_KEYCODE_2, OUTPUT_POINTER };
static Output SDLstuff_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
  [ OUTPUT_KEYCODE ] = { .type_label = "Keycode_msg", .destination = 0L },
  [ OUTPUT_KEYCODE_2 ] = { .type_label = "Keycode_msg_2", .destination = 0L },
  [ OUTPUT_POINTER ] = { .type_label = "Pointer_event", .destination = 0L },
};

enum { RENDER_MODE_GL, RENDER_MODE_OVERLAY, RENDER_MODE_SOFTWARE };

typedef struct {
  int initialized;
  int renderMode;
  int videoOk;
  int inFrames;
  int vsync;
  int toggle_fullscreen;
  int fullscreen;

  struct timeval tv_sleep_until;
  struct timeval tv_last;

  SDL_Surface *surface;
  int width;
  int height;

  char *label;
  int label_set;

  /* Overlay, used by XVideo under Linux. */
  SDL_Overlay *overlay;

  /* GL stuff. */
  struct {
    int ortho;
    int wireframe;
    int fov;
  } GL;
} SDLstuff_private;

static void _reset_video(SDLstuff_private *priv, const char *func);
#define reset_video(priv) _reset_video(priv, __func__);


static void Keycode_handler(Instance *pi, void *msg)
{
  SDLstuff_private *priv = pi->data;
  Keycode_message *km = msg;
  int handled = 0;
  
  if (km->keycode == CTI__KEY_F) {
    priv->toggle_fullscreen = 1;
    handled = 1;
  }
  
  else if (km->keycode == CTI__KEY_Q) {
    handled = 1;
    exit(0);
  }
  
  if (!handled && pi->outputs[OUTPUT_KEYCODE_2].destination) {
    /* Pass along... */
    PostData(km, pi->outputs[OUTPUT_KEYCODE_2].destination);
  }
  else {
    Keycode_message_cleanup(&km);
  }
}


static int set_label(Instance *pi, const char *value)
{
  SDLstuff_private *priv = pi->data;
  
  if (priv->label) {
    free(priv->label);
  }

  priv->label = strdup(value);
  printf("*** label set to %s\n", priv->label);

  priv->label_set = 0;
  return 0;
}

static int set_mode(Instance *pi, const char *value)
{
  SDLstuff_private *priv = pi->data;
  int oldMode = priv->renderMode;

  printf("%s: setting mode to %s\n", __FILE__, value);

  if (streq(value, "GL")) {
    priv->renderMode = RENDER_MODE_GL;
  }
  else if (streq(value, "OVERLAY")) {
    priv->renderMode = RENDER_MODE_OVERLAY;
  }
  else if (streq(value, "SOFTWARE")) {
    priv->renderMode = RENDER_MODE_SOFTWARE;
  }

  if (oldMode != priv->renderMode) {
    reset_video(priv);
  }

  return 0;
}


static int set_width(Instance *pi, const char *value)
{
  SDLstuff_private *priv = pi->data;
  int oldWidth = priv->width;
  int newWidth = atoi(value);

  if (oldWidth != newWidth) {
    printf("width: %d -> %d\n", oldWidth, newWidth);
    priv->width = newWidth;
    // reset_video(priv);
  }

  return 0;
}


static int set_height(Instance *pi, const char *value)
{
  SDLstuff_private *priv = pi->data;
  int oldHeight = priv->height;
  int newHeight = atoi(value);

  if (oldHeight != newHeight) {
    printf("height: %d -> %d\n", oldHeight, newHeight);
    priv->height = newHeight;
    reset_video(priv);
  }

  return 0;
}


static int set_fullscreen(Instance *pi, const char *value)
{
  /* Set this before video setup. */
  SDLstuff_private *priv = pi->data;
  priv->fullscreen = atoi(value);
  return 0;
}


static Config config_table[] = {
  { "label", set_label, 0L, 0L},
  { "mode", set_mode, 0L, 0L},
  { "width", set_width, 0L, 0L},
  { "height", set_height, 0L, 0L},
  { "fullscreen", set_fullscreen, 0L, 0L},
};


static void _gl_setup(SDLstuff_private *priv)
{
  const uint8_t *_e = glGetString(GL_EXTENSIONS);
  char *extensions = 0L;
  if (_e) {
    extensions = strdup((const char *)_e);
    int i;
    for(i=0; extensions[i]; i++) {
      if (extensions[i] == ' ') {
	extensions[i] = '\n';
      }
    }
    
    { 
      FILE *f = fopen("extensions.txt", "w");
      fputs(extensions, f);
      fclose(f);
    }
  }

  glViewport(0, 0, priv->width, priv->height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  //gluOrtho2D(0.0f, (GLfloat) priv->width, 0.0f, (GLfloat) priv->height);
  //gluOrtho2D(0.0f, (GLfloat) priv->width, (GLfloat) priv->height, 0.0f);
  glOrtho(0.0f, (GLfloat) priv->width, 0.0f, (GLfloat) priv->height, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void _reset_video(SDLstuff_private *priv, const char *func)
{
  /* See notes about this   */
  int rc;
  Uint32 sdl_vid_flags = 0;

  SDL_putenv("SDL_VIDEO_CENTERED=center"); //Center the game Window.

  if (priv->fullscreen) {
    sdl_vid_flags |= SDL_FULLSCREEN;
  }

  if (priv->renderMode == RENDER_MODE_GL) {
    rc = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    printf("set SDL_GL_DOUBLEBUFFER returns %d\n", rc);

    if (priv->vsync) {
      rc = SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
      printf("set SDL_GL_SWAP_CONTROL returns %d\n", rc);
    }

    sdl_vid_flags |= SDL_OPENGL;
    priv->surface = SDL_SetVideoMode(priv->width, priv->height, 32, 
				    sdl_vid_flags
				    );
  }

  else if (priv->renderMode == RENDER_MODE_OVERLAY) {
    sdl_vid_flags |= SDL_HWSURFACE | SDL_HWPALETTE | SDL_ANYFORMAT;
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 0,
				    sdl_vid_flags
				    );

    printf("[%s] reset_video(%d, %d)\n", func, priv->width, priv->height);
  }

  else  {
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 24, 
				    sdl_vid_flags
				    );
  }

  if (!priv->surface) {
    printf("SetVideoMode failed!\n");
    return;
  }

  if (priv->renderMode == RENDER_MODE_GL) {
    /* Check if got wanted attributes. */
    int v;
    rc = SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &v);
    if (0 == rc) {
      printf("SDL_GL_DOUBLEBUFFER=%d\n", v);
    }
    
    rc = SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &v);
    if (0 == rc) {
      printf("SDL_GL_SWAP_CONTROL=%d\n", v);
    }
    _gl_setup(priv);
  }

  if (priv->overlay) {
    /* Delete the overlay, it will get recreated if needed. */
    SDL_FreeYUVOverlay(priv->overlay);
    priv->overlay = 0L;
  }

  priv->videoOk = 1;
}


static void _sdl_init(SDLstuff_private *priv)
{
  int rc;

  if (getenv("DISPLAY") == NULL) {
    fprintf(stderr, "DISPLAY not set, skipping SDL_Init()\n");
    return;
  }

  rc = SDL_Init(SDL_INIT_VIDEO);
  printf("SDL_INIT_VIDEO returns %d\n", rc);

  const SDL_VideoInfo *vi = SDL_GetVideoInfo();
  if (!vi) {
    return;
  }

  printf("%dKB video memory\n", vi->video_mem);
  printf("hardware surfaces: %s\n", vi->hw_available ? "yes" : "no");

  reset_video(priv);
}


static void render_frame_gl(SDLstuff_private *priv, RGB3_buffer *rgb3_in)
{
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);

  glClear(GL_COLOR_BUFFER_BIT);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("sdl %ld.%06ld\n", tv.tv_sec, tv.tv_usec);
  }

  /* In OpenGL coordinates, Y moves "up".  Set raster pos, and use zoom to flip data vertically.  */
  /* NOTE: Adjust these accordingly if window is resized. */
  /* On butterfly, and maybe aria back when it was using the Intel
     driver, without the "-1" it renders *nothing* except for the
     clear color as set above. */
  //glRasterPos2i(0, priv->height);
  //glRasterPos2i(0, priv->height-1);
  glRasterPos2f(0.0, priv->height-0.0001);

  RGB3_buffer *rgb_final;

  rgb_final = rgb3_in;

  /* 
   * Interestingly, the order of calling glPixelZoom and glDrawPixels doesn't seem to matter. 
   * Maybe its just setting up parts of the GL pipeline before it runs.
   */
  glDrawPixels(rgb_final->width, rgb_final->height, GL_RGB, GL_UNSIGNED_BYTE, rgb_final->data);

  /* Zoom parameters should fit rgb_final image to window. */
  glPixelZoom(1.0*priv->width/rgb_final->width, -1.0*priv->height/rgb_final->height);

  /* swapBuffers should do an implicit flush, so no need to call glFlush().
     But glFinish helps a lot on software renderer (butterfly) */
  glFinish();
  SDL_GL_SwapBuffers();
}

static void render_frame_overlay(SDLstuff_private *priv, Y422P_buffer *y422p_in)
{
  if (!priv->overlay) {
    int i;

    /* FIXME: IYUV and other formats are also available!  I might need to 
       set up CJpeg on the encoding end to create something easy to display
       on butterfly's XVideo channel.  */
    priv->overlay = SDL_CreateYUVOverlay(y422p_in->width, y422p_in->height, 
					 SDL_YV12_OVERLAY, 
					 // SDL_IYUV_OVERLAY, 
					 priv->surface);
    if (!priv->overlay) {
      fprintf(stderr, "SDL_error %s\n", SDL_GetError());
      exit(1);
    }

    if (priv->overlay->planes != 3) {
      fprintf(stderr, "gah, only got %d planes in overlay, wanted 3\n", priv->overlay->planes);
      exit(1);
    }

    fprintf(stderr, 
	    "Overlay:\n"
	    "  %d, %d\n"
	    "  %d planes\n"
	    "  %s\n"
	    "", priv->overlay->w, priv->overlay->h, priv->overlay->planes,
	    priv->overlay->hw_overlay ? "hardware accelerated":"*** software only!");
    for (i=0; i < priv->overlay->planes; i++) {
      fprintf(stderr, "  %d:%p\n", priv->overlay->pitches[i], priv->overlay->pixels[i]);
    }

    fprintf(stderr, "  priv size: %d,%d\n", priv->width, priv->height);
    fprintf(stderr, "  y422p_in: %d,%d,%d\n", y422p_in->y_length, y422p_in->cb_length, y422p_in->cr_length);
    
  }

  SDL_Rect rect = { 0, 0, priv->width, priv->height };

  int iy, next_iy;		/* Start line, to allow rendering interlace fields. */
  int dy;			/* Line increment. */
  int n;			/* Number of passes, to allow for rendering 2 fields. */

  switch (y422p_in->c.interlace_mode) {
  case IMAGE_INTERLACE_NONE:
    iy = 0;
    next_iy = 0;
    dy = 1;
    n = 1;
    break;
  case IMAGE_INTERLACE_TOP_FIRST:
    iy = 0;
    next_iy = 1;
    dy = 2;
    n = 2;
    break;
  case IMAGE_INTERLACE_BOTTOM_FIRST:
    iy = 1;
    next_iy = 0;
    dy = 2;
    n = 2;
    break;
  }

  while (n) {
    SDL_LockSurface(priv->surface);
    SDL_LockYUVOverlay(priv->overlay);

    /* Copy Y */
    int img_pitch = priv->surface->w;
    if (0 && priv->overlay->pitches[0] == img_pitch) {
      memcpy(priv->overlay->pixels[0], y422p_in->y, rect.w*rect.h);
    }
    else {
      /* Pitch may be different if width is not evenly divisible by 4.
	 So its a bunch of smaller memcpys instead of a single bigger
	 one. */
      int y;
      unsigned char *src = y422p_in->y + (iy * img_pitch);
      unsigned char *dst = priv->overlay->pixels[0] + (iy * priv->overlay->pitches[0]);
      for (y=iy; y < priv->surface->h; y += dy) {
	memcpy(dst, src, img_pitch);
	src += img_pitch * dy;
	dst += (priv->overlay->pitches[0] * dy);
      }
    }

    /* Copy Cr and Cb, while basically doing 422 to 420. */
    if (1){
      int x, y, src_index, dst_index, src_width;
      src_width = y422p_in->width/2;
      src_index = src_width * iy * 2;
      for (y=iy; y < (y422p_in->height/2); y+=dy) {
	dst_index = y * priv->overlay->pitches[1];
	for (x=0; x < src_width; x++) {
	  priv->overlay->pixels[1][dst_index] = ((y422p_in->cr[src_index]+y422p_in->cr[src_index+src_width])/2);
	  priv->overlay->pixels[2][dst_index] = ((y422p_in->cb[src_index]+y422p_in->cb[src_index+src_width])/2);
	  //priv->overlay->pixels[1][dst_index] = -63;
	  //priv->overlay->pixels[2][dst_index] = -63;
	  dst_index += 1;
	  src_index += 1;
	}
	/* Skip an input line since it was used in the averaging above. */
	src_index += src_width;
	if (dy == 2) {
	  /* Interlaced field, skip 2 more input lines. */
	  src_index += src_width * 2;
	}
      }
    }
  
    SDL_UnlockYUVOverlay(priv->overlay);

    SDL_DisplayYUVOverlay(priv->overlay, &rect);

    SDL_UnlockSurface(priv->surface);

    if (n) {
      /* FIXME: ~1/60 field time is only for TV sources. */
      nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/100}, NULL);
      iy = next_iy;
      n -= 1;
    }
  }
}

static void render_frame_software(SDLstuff_private *priv, BGR3_buffer *bgr3_in)
{
  SDL_LockSurface(priv->surface);

  if (bgr3_in &&
      priv->surface->w == bgr3_in->width &&
      priv->surface->h == bgr3_in->height &&
      priv->surface->format->BitsPerPixel == 24) {
    // printf("rendering...\n");
    int img_pitch = priv->surface->w * 3;
    if (priv->surface->pitch == img_pitch) {
      memcpy(priv->surface->pixels, bgr3_in->data, bgr3_in->data_length);
    }
    else {
      /* Pitch may be different if width is not evenly divisible by 4.  */
      int y;
      unsigned char *src = bgr3_in->data;
      unsigned char *dst = priv->surface->pixels;
      for (y=0; y < priv->surface->h; y++) {
	memcpy(dst, src, img_pitch);
	src += img_pitch;
	dst += priv->surface->pitch;
      }
    }
    SDL_UpdateRect(priv->surface, 0, 0, priv->surface->w, priv->surface->h);
  }
  else {
    printf("wrong BitsPerPixel=%d\n", priv->surface->format->BitsPerPixel);
  }


  SDL_UnlockSurface(priv->surface);
}


static void pre_render_frame(SDLstuff_private *priv, int width, int height)
{
  dpf("frame %d ready for display\n", priv->inFrames);

  if (priv->toggle_fullscreen) {
    priv->fullscreen = (priv->fullscreen + 1) % 2;
    priv->toggle_fullscreen = 0;
    reset_video(priv);
  }

  if (priv->renderMode != RENDER_MODE_GL &&
      /* priv->renderMode != RENDER_MODE_OVERLAY && */
      (priv->width != width || priv->height != height)) {
    priv->width = width;
    priv->height = height;
    reset_video(priv);
  }

  if (!priv->label_set) {
    if (priv->label) {
      SDL_WM_SetCaption(priv->label, NULL); //Sets the Window Title
      priv->label_set = 1;
    }
  }
}

static void post_render_frame(Instance *pi)
{
  SDLstuff_private *priv = pi->data;
  if (pi->outputs[OUTPUT_FEEDBACK].destination) {
    Feedback_buffer *fb = Feedback_buffer_new();
    /* FIXME:  Maybe could get propagate sequence and pass it back here... */
    fb->seq = 0;
    PostData(fb, pi->outputs[OUTPUT_FEEDBACK].destination);
  }
  priv->inFrames += 1;
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Y422P_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  Y422P_buffer *y422p = data;
  BGR3_buffer *bgr3 = NULL;
  RGB3_buffer *rgb3 = NULL;

  pre_render_frame(priv, y422p->width, y422p->height);

  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      rgb3 = Y422P_to_RGB3(y422p);
      render_frame_gl(priv, rgb3);
      RGB3_buffer_discard(rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      render_frame_overlay(priv, y422p);
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      bgr3 = Y422P_to_BGR3(y422p);
      render_frame_software(priv, bgr3);
    }
    break;
  }

  post_render_frame(pi);


  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }

  if (y422p) {
    Y422P_buffer_discard(y422p);
  }

  if (bgr3) {
    BGR3_buffer_discard(bgr3);
  }
}


static void RGB3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  RGB3_buffer *rgb3 = data;
  Y422P_buffer *y422p = NULL;
  BGR3_buffer *bgr3 = NULL;

  if (0) {
    static int n = 0;
    n += 1;
    if (n == 10) {
      printf("Saving frame %d as ppm\n", n);
      FILE *f;
      f = fopen("x.ppm", "wb");
      if (f) {
	fprintf(f, "P6\n%d %d\n255\n", rgb3->width, rgb3->height);
	if (fwrite(rgb3->data, rgb3->width * rgb3->height *3, 1, f) != 1) { perror("fwrite"); }
	fclose(f);
      }
    }
  }

  pre_render_frame(priv, rgb3->width, rgb3->height);

  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      y422p = RGB3_toY422P(rgb3);
      render_frame_overlay(priv, y422p);
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      rgb3_to_bgr3(&rgb3, &bgr3);
      render_frame_software(priv, bgr3);
    }
    break;
  }

  post_render_frame(pi);  

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }

  if (y422p) {
    Y422P_buffer_discard(y422p);
  }

  if (bgr3) {
    BGR3_buffer_discard(bgr3);
  }

}


static void BGR3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  BGR3_buffer *bgr3 = data;
  RGB3_buffer *rgb3 = NULL;
  Y422P_buffer *y422p = NULL;

  pre_render_frame(priv, bgr3->width, bgr3->height);
  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      /* FIXME: glDrawPixels can handle GL_BGR, so could just pass that along...*/
      bgr3_to_rgb3(&bgr3, &rgb3);
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      /* FIXME... */
      // render_frame_overlay(priv, y422p);
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      render_frame_software(priv, bgr3);
    }
    break;
  }
  post_render_frame(pi);  

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }

  if (y422p) {
    Y422P_buffer_discard(y422p);
  }

  if (bgr3) {
    BGR3_buffer_discard(bgr3);
  }
}


static void GRAY_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  Gray_buffer *gray = data;
  BGR3_buffer *bgr3 = NULL;
  RGB3_buffer *rgb3 = NULL;
  Y422P_buffer *y422p = NULL;

  pre_render_frame(priv, gray->width, gray->height);
  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      RGB3_buffer *rgb3 = RGB3_buffer_new(gray->width, gray->height);
      int i;
      int j = 0;
      /* FIXME: Do this efficiently...*/
      for (i=0; i < gray->width*gray->height; i++) {
	rgb3->data[j] = rgb3->data[j+1] = rgb3->data[j+2] = gray->data[i];
	j += 3;
      }
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      Y422P_buffer *y422p = Y422P_buffer_new(gray->width, gray->height);
      memcpy(y422p->y, gray->data, gray->data_length);
      memset(y422p->cb, 128, y422p->cb_length);
      memset(y422p->cr, 128, y422p->cr_length);
      render_frame_overlay(priv, y422p);
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      BGR3_buffer *bgr3 = BGR3_buffer_new(gray->width, gray->height);
      int i;
      int j = 0;
      /* FIXME: Do this efficiently...*/
      for (i=0; i < gray->width*gray->height; i++) {
	bgr3->data[j] = bgr3->data[j+1] = bgr3->data[j+2] = gray->data[i];
	j += 3;
      }
      render_frame_software(priv, bgr3);
    }
    break;
  }
  post_render_frame(pi);  

  if (gray) {
    Gray_buffer_discard(gray);
  }

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }

  if (y422p) {
    Y422P_buffer_discard(y422p);
  }

  if (bgr3) {
    BGR3_buffer_discard(bgr3);
  }
}


static int my_event_loop(void *data)
{
  /* This needs to run in the context of the main application thread, for
     certain platforms (OSX in particular). */
  Handler_message *hm;
  SDL_Event ev;
  int rc;
  Instance *pi = data;

  SDLstuff_private *priv = Mem_calloc(1, sizeof(*priv));

  priv->width = 640;
  priv->height = 480;
  priv->GL.fov = 90;
  priv->vsync = 1;
  //priv->renderMode = RENDER_MODE_GL;
  //priv->renderMode = RENDER_MODE_SOFTWARE;
  priv->renderMode = RENDER_MODE_OVERLAY;
  pi->data = priv;

  printf("%s started\n", __func__);

  /* NOTE: The thread that calls SDL_WaitEvent() must be the same thread that
     sets up the video.  Both happen in this function. */

  _sdl_init(priv);

  while (1) {
    rc = SDL_WaitEvent(&ev);
    if (!rc) {
      fprintf(stderr, "SDL_error %d %s\n", rc, SDL_GetError());
      continue;
    }
    if (ev.type == SDL_USEREVENT) {
      hm = ev.user.data1;
      hm->handler(pi, hm->data);
      ReleaseMessage(&hm);
    }
    else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
      if (pi->outputs[OUTPUT_POINTER].destination) {
	Pointer_event *p = Pointer_event_new(ev.button.x,ev.button.y,
					     ev.button.button, ev.button.state);
	PostData(p, pi->outputs[OUTPUT_POINTER].destination);
      }
    }
    else if (ev.type == SDL_KEYUP) {
      /* Handle ranges here. */
      if (pi->outputs[OUTPUT_KEYCODE].destination) {
	PostData(Keycode_message_new(SDLtoCTI_Keymap[ev.key.keysym.sym]),
		 pi->outputs[OUTPUT_KEYCODE].destination);
      }
    }
    else if (ev.type == SDL_QUIT) {
      exit(0);
    }

  }

  return 0;
}


static void SDLstuff_tick(Instance *pi)
{
  /* This is called from the "SDLstuff" instance thread.  Config
     messages are passed to the main thread. */
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm && hm->handler == Config_handler) {
    printf("%s got a config message\n", __func__);
  }

  if (hm) {
    SDL_Event ev = {  .user.type = SDL_USEREVENT, 
		      .user.code = 0,
		      .user.data1 = hm,  
		      .user.data2 = 0L 
    };

    /* PushEvent will fail until SDL is initialized in the other thread. */
    while (SDL_PushEvent(&ev) == -1) {
      // printf("could not push event, retrying in 100ms...\n");
      SDL_Delay(100);
    }
  }
}

static void SDLstuff_instance_init(Instance *pi)
{
  extern Callback *ui_callback;	/* cti_app.c */
  Callback_fill(ui_callback, my_event_loop, pi);
}


static Template SDLstuff_template = {
  .label = "SDLstuff",
  .inputs = SDLstuff_inputs,
  .num_inputs = table_size(SDLstuff_inputs),
  .outputs = SDLstuff_outputs,
  .num_outputs = table_size(SDLstuff_outputs),
  .tick = SDLstuff_tick,
  .instance_init = SDLstuff_instance_init,  
};

void SDLstuff_init(void)
{
  Template_register(&SDLstuff_template);
}
