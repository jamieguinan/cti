new MjpegDemux mjd
new AAC aenc
new Splitter sp

connect mjd Wav_buffer aenc

config mjd input 192.168.2.75:6666
config mjd retry 1
config mjd enable 1
