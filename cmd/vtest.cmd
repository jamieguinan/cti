new V4L2Capture vc
new Null null
config vc device UVC
config vc format MJPG

connect vc Jpeg_buffer null

config vc enable 1
