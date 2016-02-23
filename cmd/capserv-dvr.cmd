new V4L2Capture vc

connect vc YUV422P_buffer sdl

config vc device em28xx
config vc format YUYV
config vc input S-Video
config vc std NTSC
config vc size 640x480
#config vc saturation 64
#config vc contrast 64

config vc enable 1

