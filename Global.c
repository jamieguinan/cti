#include <X11/Xlib.h>
#include <stdio.h>

#include "Global.h"
#include "localptr.h"

Global_t global = {};

void Display_free(Display **pdpy) {
  if (pdpy && *pdpy) {
    XCloseDisplay(*pdpy);
    *pdpy = NULL;
  }
}


static void x11_info()
{
  if (!getenv("DISPLAY")) {
    return;
  }

  localptr(Display, dpy) = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "Could not open X display\n");
    return;
  }

  Screen * rootscr = XDefaultScreenOfDisplay(dpy);
  global.display.width = XWidthOfScreen(rootscr);
  global.display.height = XHeightOfScreen(rootscr);

  fprintf(stderr, "global display %d, %d\n"
	  , global.display.width
	  , global.display.height
	  );
}

void Global_init()
{
  x11_info();
}
