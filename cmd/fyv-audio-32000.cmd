new MjpegDemux mjd
new ALSAPlayback ap

connect mjd Wav_buffer ap

config ap useplug 1
#config ap device plughw:0
#config ap device Dummy
config ap device UA25
#config ap device CK804
config ap rate 32000
config ap channels 1
config ap format signed.16-bit.little.endian
config ap frames_per_io 128


config mjd input 192.168.2.75:6666
#config mjd retry 1
config mjd enable 1
config mjd exit_on_eof 1

config ap enable 1

