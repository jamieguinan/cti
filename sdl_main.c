/* This provides a function call path between SDLMain.m and cti_main.c in the OSX/Cocoa build.
   Maybe I could have just called cti_main() directly from SDLMain.m, but I wanted to leave
   that module unchanged. */
extern int cti_main(int argc, char *argv[]);

int SDL_main(int argc, char *argv[])
{
  return cti_main(argc, argv);
}
