new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl

connect vc Jpeg_buffer dj
connect dj RGB3_buffer sdl

config vc device /dev/video0
config vc format MJPG
config vc size 640x480
config vc fps 30
config vc autoexpose 1
config vc exposure 300
config vc enable 1
