new MjpegDemux mjd
new DJpeg dj
new H264 venc
#new MpegTSMux mtsm

connect mjd Jpeg_buffer dj
connect dj 420P_buffer venc

config mjd input 192.168.2.77:6666
config mjd retry 1
config mjd enable 1
