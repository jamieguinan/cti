new V4L2Capture vc
new CJpeg cj
new JpegFiler jf

connect vc YUV422P_buffer cj
connect cj Jpeg_buffer jf

config vc device UVC
config vc format YUYV
config vc size 640x360
config vc fps 15

config jf prefix /tmp/logitech

config vc enable 1

