# mt
new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl
new Null null
new HTTPServer http
new VirtualStorage vs
new Splitter sp

connect vc Jpeg_buffer sp
connect sp:Jpeg_buffer.1 vs:Jpeg_buffer
connect sp:Jpeg_buffer.2 dj:Jpeg_buffer

connect dj RGB3_buffer sdl
#connect dj YUV422P_buffer sdl

#connect dj YUV422P_buffer null
#connect dj RGB3_buffer null

config vc device UVC
config vc format MJPG
config vc size 640x480
config vc fps 30
#config vc fix 1

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

config sdl mode OVERLAY

config vc Exposure,.Auto.Priority 0

config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 400

config vc enable 1

config http v4port 8010
config http enable 1
config http virtual_storage vs

