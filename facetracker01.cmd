new V4L2Capture vc
new DJpeg dj
new FaceTracker ft
new SDLstuff sdl

connect vc Jpeg_buffer dj
connect dj GRAY_buffer ft

# Separate gray path.
connect vc GRAY_buffer ft

connect ft GRAY_buffer sdl
#connect dj RGB3_buffer sdl

config vc device UVC
config vc format YUYV
config vc size 640x480
config vc fps 30

#config vc autoexpose 3

config vc autoexpose 1
config vc exposure 300

config vc enable 1
