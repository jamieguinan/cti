new V4L2Capture vc
new Null null

connect vc Jpeg_buffer null

config vc device /dev/video1
config vc format MJPG
config vc size 640x480
config vc fps 15
config vc autoexpose 3
config vc enable 1

config vc enable 1
