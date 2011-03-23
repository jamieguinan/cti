new MjpegDemux mjd
new DJpeg dj
new JpegTran jt

new H264 h
new MpegTSMux tsm

# Demuxer setup.
config mjd use_feedback 0
config mjd input 192.168.2.123:6666

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj 422P_buffer h
connect h H264_buffer tsm

# Enable mjd to start the whole thing running.
config mjd enable 1
