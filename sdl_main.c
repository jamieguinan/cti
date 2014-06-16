extern void sdl_event_loop(void);

int SDL_main(int argc, char *argv[])
{
  sdl_event_loop();
  return 0;
}
