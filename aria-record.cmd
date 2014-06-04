new MjpegDemux mjd
new MjpegMux mjm

connect mjd Jpeg_buffer mjm
connect mjd Wav_buffer mjm

config mjd input 192.168.2.22:6667

config mjm output /av/media/tmp/aria-%Y%m%d-%H%M%S.mjx

config mjd enable 1
