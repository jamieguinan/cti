new V4L2Capture vc
new RPiH264Enc enc
new BinaryFiler bf

connect vc YUV422P_buffer enc
connect enc H264_buffer bf

config bf output /dev/shm/test.h264

config vc device /dev/video0
config vc format YUYV
config vc size 640x480
config vc fps 15
config vc autoexpose 3

config vc enable 1
