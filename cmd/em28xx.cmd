new V4L2Capture vc
new SDLstuff sdl

connect sdl:Keycode_msg sdl:Keycode_msg

connect vc YUV422P_buffer sdl

config sdl mode OVERLAY
#config sdl mode SOFTWARE

config vc device em28xx
config vc format YUYV
#config vc input S-Video
config vc input Composite1
config vc std NTSC
config vc size 640x480
config vc saturation 64
config vc contrast 64

config vc enable 1

