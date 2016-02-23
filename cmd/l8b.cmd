v 0
new MjpegDemux mjd
new Null null
new SDLstuff sdl
# new JpegTran jt

connect mjd Jpeg_buffer dj

config mjd input 127.0.0.1:6666
config mjd enable 1
