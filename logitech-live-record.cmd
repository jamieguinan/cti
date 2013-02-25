new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl
new MjpegMux mjm

# The jpeg/rgb path can be there even if it isn't used.
connect vc Jpeg_buffer dj
connect dj RGB3_buffer sdl
connect dj Jpeg_buffer mjm

config vc device 081b
config vc format MJPG
config vc size 640x480
#config vc size 800x600
config vc fps 30

#config vc autoexpose 3

config vc autoexpose 1
config vc exposure 300

connect sdl:Keycode_msg vc:Keycode_msg

config mjm output logitech-live-record-%s.mjx

config vc enable 1
