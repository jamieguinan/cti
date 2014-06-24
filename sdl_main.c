/* I am keeping this for compatibility with SDLMain.m and basic SDL
   conventions that call for SDL_main() to be defined somewhere.  
   This version calls back into SDLstuff.c in the context of the main
   application thread. */
extern void sdl_event_loop(void);

int SDL_main(int argc, char *argv[])
{
  sdl_event_loop();
  return 0;
}
