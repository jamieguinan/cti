new V4L2Capture vc
new DJpeg dj
#new SDLstuff sdl
new MjpegMux mjm

connect vc Jpeg_buffer mjm

#connect vc Jpeg_buffer dj
#connect dj RGB3_buffer sdl

config mjm output cap-%Y%m%d-%H%M%S.mjx

#config vc drivermatch gspca_topro
config vc device gspca
config vc format JPEG
config vc size 640x480
config vc fps 10

config vc enable 1
v 3
