new Y4MInput yi
new H264 venc

connect yi YUV420P_buffer venc

config venc output /tmp/test.h264
config venc preset faster
#config venc tune psnr
config venc tune zerolatency
#config venc profile baseline
config venc cqp 25
#config venc crf 20
# Assuming 15fps, generate a keyframe every 5 seconds.
config venc keyint_max 75

config yi input /tmp/test.y4m
config yi exit_on_eof 0
config yi enable 1

