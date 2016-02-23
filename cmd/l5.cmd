new V4L2Capture vc
new JpegTran jt
new JpegFiler jf

connect vc Jpeg_buffer jt
connect jt Jpeg_buffer jf

config vc device /dev/video1
config vc format MJPG
config vc size 640x480
config vc fps 15
config vc autoexpose 3
# config vc exposure 150

config vc enable 1
