new V4L2Capture vc
new SDLstuff sdl
new Splitter sp
new H264 venc

config sdl mode OVERLAY
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

config vc device BT878

# 422P requires calls to YUV422P_to_YUV420P, but looks 
# cleaner than YU12
config vc format 422P
connect vc YUV422P_buffer sp
connect sp:YUV422P_buffer.1 venc:YUV422P_buffer
connect sp:YUV422P_buffer.2 sdl:YUV422P_buffer

# YU12 works, but has noticable color fringing comparted to 422P.
#config vc format YU12
#connect vc YUV420P_buffer sdl

config venc output vidcap.264
# faster fast medium
config venc preset faster
config venc tune psnr
# baseline main high
config venc profile baseline

config vc std NTSC
config vc size 640x480
config vc input Composite1
config vc mute 0
#config vc Contrast 63
config vc enable 1

