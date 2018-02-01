new Y4MInput yi
new RPiH264Enc enc
new BinaryFiler bf

connect yi YUV420P_buffer enc
connect enc H264_buffer bf

config enc bitrate $BITRATE
config enc gop_seconds 1

config bf output /dev/shm/test.h264

config yi input /dev/shm/y4m.raw
config yi enable 1
