new V4L2Capture vc
new SDLstuff sdl
new UI001 ui
new FaceTracker ft
new Null null

#connect vc YUV422P_buffer sdl
#connect vc GRAY_buffer sdl

connect vc GRAY_buffer ft
#connect vc GRAY_buffer sdl

connect ft GRAY_buffer sdl
connect ft RGB3_buffer sdl

connect sdl Pointer_event ui

config ui add_button text=Set;width=200;height=150;bgcolor=0xff0000

config vc device /dev/video0
config vc format YUYV
#config vc format MJPG
#config vc size 160x120
config vc size 320x240
#config vc size 640x480
#config vc size 800x600
#config vc fps 15
config vc fps 30

#config vc autoexpose 3

config vc autoexpose 1
config vc exposure 300
config vc Gain 100

config ft iir_decay 0.8
config ft chop 10

connect sdl:Keycode_msg vc:Keycode_msg

config vc enable 1
