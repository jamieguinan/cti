new MjpegDemux mjd
#new DJpeg dj
#new H264 venc
#new MpegTSMux mtsm
new AAC aenc

#connect mjd Jpeg_buffer dj
connect mjd Jpeg_buffer dj
connect mjd Wav_buffer aenc
#connect dj YUV420P_buffer venc

config mjd input 192.168.2.77:6666
config mjd retry 1
config mjd enable 1
