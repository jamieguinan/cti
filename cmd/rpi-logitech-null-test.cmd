new V4L2Capture vc
new RPiH264Enc enc
new Null null

connect vc YUV422P_buffer enc
connect enc H264_buffer null

config vc device /dev/video0
config vc format YUYV
config vc size 640x360
config vc fps 15
config vc autoexpose 3

config vc enable 1

