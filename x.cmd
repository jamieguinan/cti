new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl

connect mjd Jpeg_buffer dj

connect sdl Feedback_buffer mjd

connect sdl:Keycode_msg sdl:Keycode_msg

#connect dj RGB3_buffer sdl
connect dj 422P_buffer sdl

config sdl mode OVERLAY
#config sdl mode GL
#config sdl mode SOFTWARE
#config sdl width 1920
#config sdl height 1080


config mjd input 192.168.2.75:6666
config mjd retry 1
config mjd enable 1

ignoreeof 1
