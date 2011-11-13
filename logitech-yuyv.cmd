new V4L2Capture vc
new DJpeg dj
new SDLstuff sdl
new UI001 ui

# The jpeg/rgb path can be there even if it isn't used.
connect vc Jpeg_buffer dj
connect dj RGB3_buffer sdl

# Testing YUYV with the gray path.
#connect vc GRAY_buffer sdl

# Testing YUYV with the YCbCr path.
connect vc 422P_buffer sdl

connect sdl Pointer_event ui

config ui add_button text=Set;width=200;height=150;bgcolor=0xff0000

# Ignore backed up frames.
config dj max_messages 2

config vc device UVC
config vc format YUYV
#config vc format MJPG
config vc size 640x480
#config vc size 800x600
config vc fps 30

#config vc autoexpose 3

config vc autoexpose 1
config vc exposure 300

connect sdl:Keycode_msg vc:Keycode_msg

config vc enable 1
