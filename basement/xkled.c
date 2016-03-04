#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <stdio.h>

int main()
{
  Bool xrc;
  int ndx = 0;
  Bool state= 0;
  Atom a;
  int lib_major = XkbMajorVersion;
  int lib_minor = XkbMinorVersion;
  int opcode, event, error;
  unsigned int bits= 0;

  Display *d = XOpenDisplay(0L); /* Use $DISPLAY */

  if (!d) {
    fprintf(stderr, "could not open display!\n");
    goto out;
  }

  xrc = XkbLibraryVersion(&lib_major, &lib_minor);
  if (xrc != True) {
    fprintf(stderr, "XkbLibraryVersion error\n");
    goto out;
  }

  xrc = XkbQueryExtension(d,
			  &opcode,
			  &event,
			  &error,
			  &lib_major,
			  &lib_minor);
  if (xrc != True) {
    fprintf(stderr, "XkbQueryExtension error\n");
    goto out;
  }

  //a = XInternAtom(d, "Caps Lock", True);
  //a = XInternAtom(d, "Num Lock", True);
  a = XInternAtom(d, "Scroll Lock", True);
  if (a == None) {
    fprintf(stderr, "atom not found!\n");
    goto out;
  }

  xrc = XkbGetNamedIndicator(d,  
			     // XkbUseCoreKbd, 
			     a,
			     NULL, 
			     &state,
			     NULL,
			     NULL);

  if (xrc != True) {
    fprintf(stderr, "XkbGetNamedIndicator error\n");
  }
  else {
    printf("state=%d\n", state);
  }

  xrc = XkbSetNamedIndicator(d,  
			     // XkbUseCoreKbd, 
			     a,
			     True,
			     state ? False: True, /* toggle */
			     False,
			     0L);
  if (xrc != True) {
    fprintf(stderr, "XkbSetNamedIndicator error\n");
  }

  xrc = XkbGetIndicatorState(d,
			     XkbUseCoreKbd,
			     &bits);

  if (xrc != Success) {
    fprintf(stderr, "XkbGetIndicatorState error\n");
  }
  else {
    printf("bits=0x%x\n", bits);
  }

 out:
  XCloseDisplay(d);
  return 0;
}

/*
 * Local Variables:
 * compile-command: "gcc xkled.c -o xkled -lX11"
 * End:
 */
