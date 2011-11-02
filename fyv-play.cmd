new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl

connect mjd Jpeg_buffer dj

connect sdl Feedback_buffer mjd

#connect dj RGB3_buffer sdl
connect dj 422P_buffer sdl

config sdl mode OVERLAY
#config sdl mode GL
#config sdl mode SOFTWARE

config mjd input 192.168.2.123:6666
config mjd retry 1
config mjd enable 1
