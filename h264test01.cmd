new DO511 do
new V4L2Capture vc
new H264 h
new MpegTSMux tsm

config vc device /dev/video1
config vc format O511
config vc size 640x480
config vc fps 15
connect vc O511_buffer do
connect do YUV420P_buffer h
connect h H264_buffer tsm

config vc enable 1
