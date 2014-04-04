#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "SDL.h"
#include <gl.h>
#include <glu.h>

int SDL_main(int argc, char *argv[])
{
  int rc;

  rc = SDL_Init(SDL_INIT_VIDEO);
  printf("SDL_INIT_VIDEO returns %d\n", rc);

  return 0;
}

/* int main(int argc, char *argv[]) */
/* { */
/*   return SDL_main(argc, argv); */
/* } */
