new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl

connect mjd Jpeg_buffer dj

connect sdl:Keycode_msg sdl:Keycode_msg

connect dj YUV420P_buffer sdl

# ifast works best on sully.
config dj dct_method ifast

#config sdl mode OVERLAY
#config sdl mode GL
config sdl mode SOFTWARE
#config sdl width 640
#config sdl height 360

#config sdl width 1280
#config sdl height 720

# config mjd output fyv-direct-capture.mjx

config mjd input 192.168.2.75:6666
config mjd retry 1
config mjd enable 1

config ap enable 1

# ignoreeof 1
