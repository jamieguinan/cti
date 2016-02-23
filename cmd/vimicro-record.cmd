new V4L2Capture vc
new CJpeg cj
new SDLstuff sdl
new Splitter sp

new MjpegMux mjm

connect vc YUV422P_buffer sp

connect sp:YUV422P_buffer.1 sdl:YUV422P_buffer
connect sp:YUV422P_buffer.2 cj:YUV422P_buffer

#connect sdl Pointer_event ui
#connect sdl mode OVERLAY

connect cj Jpeg_buffer mjm

config mjm output /tmp/vimicro-%Y%m%d-%H%M%S.mjx

config vc device Vimicro
config vc format YUYV
config vc size 640x480
config vc fps 30

#config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 300

connect sdl:Keycode_msg vc:Keycode_msg
connect sdl:Keycode_msg_2 sdl:Keycode_msg

config vc enable 1
