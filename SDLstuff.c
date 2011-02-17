#ifndef __ARMEB__

#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>
#include <math.h>		/* fabs */
#include <SDL.h>
#include <gl.h>
// #include <glu.h>

#include "Template.h"
#include "Images.h"
#include "Cfg.h"

#define _min(a, b)  ((a) < (b) ? (a) : (b))
#define _max(a, b)  ((a) > (b) ? (a) : (b))

static void Config_handler(Instance *pi, void *data);
static void Y422P_handler(Instance *pi, void *data);
static void RGB3_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_422P, INPUT_RGB3, INPUT_BGR3 };
static Input SDLstuff_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_422P ] = { .type_label = "422P_buffer", .handler = Y422P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
};

enum { OUTPUT_FEEDBACK, OUTPUT_CONFIG };
static Output SDLstuff_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
};

enum { RENDER_MODE_GL, RENDER_MODE_OVERLAY, RENDER_MODE_SOFTWARE };

typedef struct {
  int initialized;
  int renderMode;
  int videoOk;
  int inFrames;
  int vsync;

  struct timeval tv_sleep_until;
  struct timeval tv_last;

  SDL_Surface *surface;
  int width;
  int height;

  long seek_amount;

  char *label;
  int label_set;

  /* Overlay stuff.  XVideo when using Linux. */
  SDL_Overlay *overlay;

  /* GL stuff. */
  struct {
    int ortho;
    int wireframe;
    int fov;
  } GL;
} SDLstuff_private;

static void reset_video(SDLstuff_private *priv);


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
    reset_video(priv);
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


static Config config_table[] = {
  { "label", set_label, 0L, 0L},
  { "mode", set_mode, 0L, 0L},
  { "width", set_width, 0L, 0L},
  { "height", set_height, 0L, 0L},
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

static void reset_video(SDLstuff_private *priv)
{
  /* See notes about this   */
  int rc;
  Uint32 sdl_vid_flags = 0;

  SDL_putenv("SDL_VIDEO_CENTERED=center"); //Center the game Window.

  if (priv->renderMode == RENDER_MODE_GL) {
    rc = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    printf("set SDL_GL_DOUBLEBUFFER returns %d\n", rc);

    if (priv->vsync) {
      rc = SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
      printf("set SDL_GL_SWAP_CONTROL returns %d\n", rc);
    }

    sdl_vid_flags |= SDL_OPENGL;
    // sdl_vid_flags |= SDL_FULLSCREEN;
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 32, 
				    sdl_vid_flags
				    );
  }

  else if (priv->renderMode == RENDER_MODE_OVERLAY) {
    sdl_vid_flags |= SDL_HWSURFACE | SDL_HWPALETTE;
    // sdl_vid_flags |= SDL_FULLSCREEN;
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 0,
				    sdl_vid_flags
				    );
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

  {
    static int n = 0;
    n += 1;
    if (n == 10) {
      printf("Saving frame %d as ppm\n", n);
      FILE *f;
      f = fopen("x.ppm", "wb");
      if (f) {
	fprintf(f, "P6\n%d %d\n255\n", rgb_final->width, rgb_final->height);
	if (fwrite(rgb_final->data, rgb_final->width * rgb_final->height *3, 1, f) != 1) { perror("fwrite"); }
	fclose(f);
      }
    }
  }

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
    priv->overlay = SDL_CreateYUVOverlay(priv->width, priv->height, 
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

    fprintf(stderr, "  y422p_in: %d,%d,%d\n", y422p_in->y_length, y422p_in->cb_length, y422p_in->cr_length);
    
  }

  SDL_Rect rect = { 0, 0, priv->width, priv->height };

  SDL_LockSurface(priv->surface);

  SDL_LockYUVOverlay(priv->overlay);

  /* Copy Y */
  memcpy(priv->overlay->pixels[0], y422p_in->y, rect.w*rect.h);

  /* Copy Cr and Cb, while basically doing 422 to 420. */
  {
    int x, y, src_index, dst_index, src_width;
    dst_index = 0;
    src_index = 0;
    src_width = priv->overlay->pitches[1];
    for (y=0; y < priv->height; y+=2) {
      for (x=0; x < priv->overlay->pitches[1]; x++) {
	priv->overlay->pixels[1][dst_index] = (y422p_in->cr[src_index]+y422p_in->cr[src_index+src_width])/2;
	priv->overlay->pixels[2][dst_index] = (y422p_in->cb[src_index]+y422p_in->cb[src_index+src_width])/2;
	dst_index += 1;
	src_index += 1;
      }
      src_index += src_width;	/* Skip a line. */
    }
  }
  
  SDL_UnlockYUVOverlay(priv->overlay);

  SDL_DisplayYUVOverlay(priv->overlay, &rect);

  SDL_UnlockSurface(priv->surface);  
}

static void render_frame_software(SDLstuff_private *priv, BGR3_buffer *bgr3_in)
{
  SDL_LockSurface(priv->surface);

  if (bgr3_in &&
      priv->surface->w == bgr3_in->width &&
      priv->surface->h == bgr3_in->height &&
      priv->surface->format->BitsPerPixel == 24) {
    // printf("rendering...\n");
    memcpy(priv->surface->pixels, bgr3_in->data, bgr3_in->data_length);
    SDL_UpdateRect(priv->surface, 0, 0, priv->surface->w, priv->surface->h);
  }
  else {
    printf("wrong BitsPerPixel=%d\n", priv->surface->format->BitsPerPixel);
  }


  SDL_UnlockSurface(priv->surface);
}

static inline double tv_to_double(struct timeval *tv) {
  return (tv->tv_sec + tv->tv_usec/1000000.0);
}

static inline int tv_lt_diff(struct timeval *tv1, struct timeval *tv2, struct timespec *diff)
{
  if (tv1->tv_sec > tv2->tv_sec ||
      (tv1->tv_sec == tv2->tv_sec && tv1->tv_usec > tv2->tv_usec)) {
    diff->tv_sec = 0;
    diff->tv_nsec = 0;
    return 0;
  }

  diff->tv_sec = tv2->tv_sec - tv1->tv_sec;
  if (tv2->tv_usec > tv1->tv_usec) {
    /* Straight subtraction. */
    diff->tv_nsec = (tv2->tv_usec - tv1->tv_usec)*1000;
  }
  else {
    /* Subtract with borrow. */
    diff->tv_nsec = (1000000 + tv2->tv_usec - tv1->tv_usec)*1000;
    diff->tv_sec -= 1;
  }

  return 1;
}

static inline int tv_gt_diff(struct timeval *tv1, struct timeval *tv2, struct timespec *diff)
{
  if (tv1->tv_sec < tv2->tv_sec ||
      (tv1->tv_sec == tv2->tv_sec && tv1->tv_usec < tv2->tv_usec)) {
    diff->tv_sec = 0;
    diff->tv_nsec = 0;
    return 0;
  }

  diff->tv_sec = tv1->tv_sec - tv2->tv_sec;
  if (tv1->tv_usec > tv2->tv_usec) {
    /* Straight subtraction. */
    diff->tv_nsec = (tv1->tv_usec - tv2->tv_usec)*1000;
  }
  else {
    /* Subtract with borrow. */
    diff->tv_nsec = (1000000 + tv1->tv_usec - tv2->tv_usec)*1000;
    diff->tv_sec -= 1;
  }

  return 1;
}

static inline void tv_ts_add(struct timeval *tv, struct timespec *ts, struct timeval *tv_out)
{
  tv_out->tv_sec = tv->tv_sec + ts->tv_sec;
  tv_out->tv_usec = tv->tv_usec + (ts->tv_nsec/1000);
  if (tv_out->tv_usec > 1000000) {
    /* Carry. */
    tv_out->tv_usec -= 1000000;
    tv_out->tv_sec += 1;
  }
}

static inline void tv_clear(struct timeval *tv)
{
  tv->tv_sec = 0;
  tv->tv_usec = 0;
}


static void handle_playback_timing(SDLstuff_private *priv, struct timeval *tv_current)
{
  struct timeval now;
  struct timespec ts;
  double d;

  d = fabs(tv_to_double(&priv->tv_last) - tv_to_double(tv_current));

  if (d > 1.0) {
    /* tv_last is unset, or tv_current has changed substantially. */
    fprintf(stderr, "d:%.3f too big\n", d);
    goto out1;
    return;
  }

  gettimeofday(&now, 0L);

  if (tv_lt_diff(&now, &priv->tv_sleep_until, &ts)) {
    fprintf(stderr, "nanosleep %ld.%09ld\n", ts.tv_sec, ts.tv_nsec);
    if (nanosleep(&ts, 0L) != 0) { perror("nanosleep"); }
  }
  else {
    // fprintf(stderr, "no sleep\n");
    /* FIXME: If we took too long more than once, should do something... */
    gettimeofday(&priv->tv_sleep_until, 0L);
  }

  if (tv_gt_diff(tv_current, &priv->tv_last, &ts)) {
    /* Normally expect this. */
    tv_ts_add(&priv->tv_sleep_until, &ts, &priv->tv_sleep_until);
    // fprintf(stderr, "normal; sleep until { %ld.%06ld } \n",  priv->tv_sleep_until.tv_sec, priv->tv_sleep_until.tv_usec);
  }
  else {
    /* tv_current is unexpectedly behind tv_last.  Clear sleep_until and hope things
       work out. */
  }

 out1:
  priv->tv_last = *tv_current;
}

static void pre_render_frame(SDLstuff_private *priv, int width, int height)
{
  if (cfg.verbosity) {
    printf("frame %d ready for display\n", priv->inFrames);
  }

  if (priv->renderMode != RENDER_MODE_GL &&
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
  int i;
  Config_buffer *cb_in = data;

  printf("%s:%s\n", __FILE__, __func__);

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      break;
    }
  }
    
  Config_buffer_discard(&cb_in);
}


static void Y422P_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  Y422P_buffer *y422p_in = data;

  //handle_playback_timing(priv, &y422p_in->tv);
    
  pre_render_frame(priv, y422p_in->width, y422p_in->height);

  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      RGB3_buffer *rgb;
      rgb = Y422P_to_RGB3(y422p_in);
      render_frame_gl(priv, rgb);
      RGB3_buffer_discard(rgb);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      render_frame_overlay(priv, y422p_in);
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      BGR3_buffer *bgr;
      bgr = Y422P_to_BGR3(y422p_in);
      render_frame_software(priv, bgr);
      BGR3_buffer_discard(bgr);
    }
    break;
  }

  post_render_frame(pi);
  Y422P_buffer_discard(y422p_in);
}


static void RGB3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  RGB3_buffer *rgb3 = data;

  //handle_playback_timing(priv, &rgb3->tv);

  pre_render_frame(priv, rgb3->width, rgb3->height);

  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY: 
    {
      /* FIXME... */
      Y422P_buffer *y422p = RGB3_toY422P(rgb3);
      render_frame_overlay(priv, y422p);
      Y422P_buffer_discard(y422p);      
    }
    break;
  case RENDER_MODE_SOFTWARE: 
    {
      BGR3_buffer *bgr3;
      rgb3_to_bgr3(&rgb3, &bgr3);
      render_frame_software(priv, bgr3);
      BGR3_buffer_discard(bgr3);
    }
    break;
  }

  post_render_frame(pi);  

  if (rgb3) {
    RGB3_buffer_discard(rgb3);
  }
}


static void BGR3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = pi->data;
  BGR3_buffer *bgr3 = data;

  //handle_playback_timing(priv, &bgr3->tv);
   
  pre_render_frame(priv, bgr3->width, bgr3->height);
  switch (priv->renderMode) {
  case RENDER_MODE_GL: 
    {
      RGB3_buffer *rgb3 = 0L;
      /* FIXME: glDrawPixels can handle GL_BGR, so could just pass that along...*/
      bgr3_to_rgb3(&bgr3, &rgb3);
      render_frame_gl(priv, rgb3);
      RGB3_buffer_discard(rgb3);
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

  if (bgr3) {
    BGR3_buffer_discard(bgr3);
  }
}


static int my_event_loop(void *data)
{
  Handler_message *hm;
  SDL_Event ev;
  int rc;
  Instance *pi = data;
  SDLstuff_private *priv = pi->data;

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
    else if (ev.type == SDL_KEYUP) {
      char numstr[64];
      switch (ev.key.keysym.sym) {
      case SDLK_UP: 
	priv->seek_amount = _min(priv->seek_amount*2, 1000000000); 
	fprintf(stderr, "seek_amount=%ld\n", priv->seek_amount);
	break;

      case SDLK_DOWN: 
	priv->seek_amount = _max(priv->seek_amount/2, 1); 
	fprintf(stderr, "seek_amount=%ld\n", priv->seek_amount);
	break;

      case SDLK_LEFT: 
	if (pi->outputs[OUTPUT_CONFIG].destination) {
	  sprintf(numstr, "%ld", -(priv->seek_amount));
	  fprintf(stderr, "seek backward %ld\n", priv->seek_amount);
	  PostData(Config_buffer_new("seek", numstr), pi->outputs[OUTPUT_CONFIG].destination);
	}
	break;

      case SDLK_RIGHT: 
	sprintf(numstr, "%ld", priv->seek_amount);
	fprintf(stderr, "seek forward %ld\n", priv->seek_amount);	  
	PostData(Config_buffer_new("seek", numstr), pi->outputs[OUTPUT_CONFIG].destination);
	break;

      default: 
	break;
      }
    }
  }

  return 0;
}


static void SDLstuff_tick(Instance *pi)
{
  Handler_message *hm;
  SDLstuff_private *priv = pi->data;

  if (!priv->initialized) {
    // Start SDL_event_loop()
    SDL_CreateThread(my_event_loop, pi);
    priv->initialized = 1;
  }

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
      printf("could not push event, retrying in 100ms...\n");
      SDL_Delay(100);
    }
  }
}


static void SDLstuff_instance_init(Instance *pi)
{
  SDLstuff_private *priv = calloc(1, sizeof(*priv));

  //priv->width = 720;
  priv->width = 640;
  priv->height = 480;
  // priv->height = 360;
  priv->GL.fov = 90;
  priv->vsync = 1;
  //priv->renderMode = RENDER_MODE_GL;
  //priv->renderMode = RENDER_MODE_SOFTWARE;
  priv->renderMode = RENDER_MODE_OVERLAY;

  priv->seek_amount = 10000000;

  pi->data = priv;
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


#else
#include <stdio.h>
/* ARMEB */
void SDLstuff_init(void)
{
  fprintf(stderr, "SDLstuff not supported in ARCH=armeb build...\n");
}

#endif
