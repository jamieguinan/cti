new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl
new MjpegMux mjm

connect vc Jpeg_buffer mjm
connect mjm Jpeg_buffer dj
connect dj RGB3_buffer sdl

config mjm output cap-%Y%m%d-%H%M%S.mjx

config vc device /dev/video3
config vc format JPEG
config vc size 640x480

config vc enable 1
