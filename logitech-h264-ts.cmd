new V4L2Capture vc
new DJpeg dj
new H264 venc

connect vc Jpeg_buffer dj
connect dj YUV420P_buffer venc

config venc preset faster
#config venc tune psnr
config venc tune zerolatency
config venc profile baseline

config venc output ltest.h264

config vc device UVC
config vc format MJPG
config vc size 640x480
config vc fps 30
config vc autoexpose 1
config vc exposure 300
config vc Exposure,.Auto.Priority 0

config vc enable 1

system sleep 10
exit
