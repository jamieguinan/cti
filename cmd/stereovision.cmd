new V4L2Capture vc1
new V4L2Capture vc2
new SDLstuff sdl
new DJpeg dj1
new DJpeg dj2
new Null null
new VMixer vm
new CJpeg cj
new JpegFiler jf

config vc1 device /dev/v4l/by-id/usb-046d_081b_CCBEA150-video-index0
config vc2 device /dev/v4l/by-id/usb-046d_081b_EC8EA150-video-index0

config vc1 Exposure,.Auto.Priority 0
config vc2 Exposure,.Auto.Priority 0

config vc1 format MJPG
config vc2 format MJPG

config vc1 size 640x480
config vc2 size 640x480

config vc1 fps 15
config vc2 fps 15

config sdl rec_key R
config sdl snapshot_key S

config vm size 1280x480
config vm xoffset 0:0
config vm yoffset 0:0
config vm xoffset 1:640
config vm yoffset 1:0

config cj quality 85

config jf prefix stereo

connect vc1 Jpeg_buffer dj1
connect vc2 Jpeg_buffer dj2

connect dj1:YUV422P_buffer vm:YUV422P_buffer_1
connect dj2:YUV422P_buffer vm:YUV422P_buffer_2

connect vm YUV422P_buffer sdl

connect sdl:Keycode_msg sdl:Keycode_msg

connect sdl YUV422P_buffer cj

connect cj Jpeg_buffer jf

config vc1 enable 1
config vc2 enable 1
