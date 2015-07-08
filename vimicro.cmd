new V4L2Capture vc
new DJpeg dj
new JpegTran jt
new SDLstuff sdl
new UI001 ui
new LinuxEvent button

# The jpeg/rgb path can be there even if it isn't used.
connect vc Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj RGB3_buffer sdl

# Testing YUYV with the gray path.
#connect vc GRAY_buffer sdl

# Testing YUYV with the YCbCr path.
connect vc YUV422P_buffer sdl

connect sdl Pointer_event ui
#connect sdl mode OVERLAY

config ui add_button text=Set;width=200;height=150;bgcolor=0xff0000

# Ignore backed up frames.
config dj max_messages 2

config vc device Vimicro
config vc format YUYV
config vc size 640x480
#config vc size 800x600
config vc fps 30

#config vc autoexpose 3

# Mirror
#config jt transform FLIP_H

#config vc autoexpose 1
#config vc exposure 300

# 'q' to quit.
connect sdl:Keycode_msg sdl:Keycode_msg

config vc enable 1

# Enable snapshot button.
config button device /dev/input/by-id/usb-Vimicro_Co._ltd_Vimicro_USB2.0_UVC_PC_Camera-event-if00
config sdl snapshot_key CAMERA
connect button Keycode_msg sdl

# Save snapshot as Jpeg.
new CJpeg cj
new JpegFiler jf
config cj quality 85
connect sdl YUV422P_buffer cj
connect cj Jpeg_buffer jf
config jf prefix vimicro_