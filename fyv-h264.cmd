new MjpegDemux mjd
new DJpeg dj
new H264 h
new Null null

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer null
connect dj 422P_buffer h

config mjd input 192.168.2.123:6666
config mjd enable 1
