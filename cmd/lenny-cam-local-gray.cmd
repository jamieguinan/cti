new V4L2Capture vc
new SDLstuff sdl

connect vc GRAY_buffer sdl

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg
config sdl snapshot_key S

config vc device Integrated
config vc format YUYV
config vc size 640x480
config vc Brightness 64
config vc Gamma 192
config vc enable 1

new CJpeg cj
new JpegFiler jf
config cj quality 85
connect sdl YUV420P_buffer cj
connect cj Jpeg_buffer jf
config jf prefix pic_
