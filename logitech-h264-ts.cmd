new V4L2Capture vc
new DJpeg dj
new JpegTran jt
new H264 h
new MpegTSMux mtm

connect vc Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj YUV422P_buffer h
connect h H264_buffer mtm

config vc device UVC
config vc format MJPG
config vc size 640x480
config vc fps 30
config vc autoexpose 1
config vc exposure 300

config vc enable 1
