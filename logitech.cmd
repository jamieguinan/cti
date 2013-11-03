new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl

connect vc Jpeg_buffer dj
connect dj RGB3_buffer sdl

config vc device 081b
config vc format MJPG
config vc size 640x480
config vc fps 30

config sdl mode OVERLAY

config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 400


config vc enable 1
