new V4L2Capture vc
new SDLstuff sdl

connect vc YUV422P_buffer sdl

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

config vc device USB
config vc format YUYV
config vc size 640x480
config vc fps 15
config vc enable 1
