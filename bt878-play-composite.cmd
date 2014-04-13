new V4L2Capture vc
new SDLstuff sdl

config sdl mode OVERLAY
connect sdl:Keycode_msg sdl:Keycode_msg

config vc device BT878

# 422P requires calls to YUV422P_to_YUV420P, but looks 
# cleaner than YU12
config vc format 422P
connect vc YUV422P_buffer sdl

# YU12 works, but has noticable color fringing comparted to 422P.
#config vc format YU12
#connect vc YUV420P_buffer sdl

config vc std NTSC
config vc size 640x480
config vc input Composite1
config vc mute 0
#config vc Contrast 63
config vc enable 1

