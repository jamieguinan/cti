new V4L2Capture vc
new SDLstuff sdl
new DJpeg dj

connect vc Jpeg_buffer dj
#connect jt Jpeg_buffer dj
connect dj RGB3_buffer sdl

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

config vc device USB
config vc format MJPG
config vc size 640x480
#config vc fps 30
config vc enable 1
