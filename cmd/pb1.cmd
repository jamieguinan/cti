new DJpeg dj
new MjpegDemux mjd
new SDLstuff sdl
config mjd input $INPUT
connect mjd Jpeg_buffer dj
connect dj RGB3_buffer sdl
config mjd enable 1
