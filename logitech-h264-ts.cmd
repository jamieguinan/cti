new V4L2Capture vc
new DJpeg dj
new JpegTran jt
new H264 h264
new MpegTSMux mtm

connect vc Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj 422P_buffer h264
connect h264 H264_buffer mtm

config vc device /dev/video2
config vc format MJPG
config vc size 640x480
config vc fps 30
config vc autoexpose 1
config vc exposure 300

config vc enable 1
