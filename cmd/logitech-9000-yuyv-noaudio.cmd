new V4L2Capture vc
new SDLstuff sdl

# Testing YUYV with the gray path.
#connect vc GRAY_buffer sdl

# Testing YUYV with the YCbCr path.
connect vc YUV422P_buffer sdl


# config vc device UVC
config vc device /dev/v4l/by-id/usb-046d_0990_95586111-video-index0
config vc format YUYV
config vc Exposure,.Auto.Priority 0
config vc size 800x600

config vc fps 15

config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 300

config sdl snapshot_key S

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

config vc enable 1
