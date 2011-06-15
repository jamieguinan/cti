new V4L2Capture vc
new DJpeg dj
new FaceTracker ft
new SDLstuff sdl

connect vc Jpeg_buffer dj
connect dj GRAY_buffer ft
connect ft GRAY_buffer sdl
#connect dj RGB3_buffer sdl

config vc device /dev/video0
config vc format MJPG
config vc size 640x480
config vc fps 30

#config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 400


config vc enable 1
