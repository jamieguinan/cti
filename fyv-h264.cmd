new MjpegDemux mjd
new DJpeg dj
new H264 h
new Null null

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer null
connect dj YUV422P_buffer h

config mjd input 192.168.2.75:6666
config mjd enable 1

config h output fyv-%Y%m%d-%H%M%S.h264
