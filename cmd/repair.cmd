# v 3
new MjpegDemux mjd
new MjxRepair mr

config mjd input /av/media/tmp/20110901-144446.mjx

connect mjd Jpeg_buffer mr
connect mjd Wav_buffer mr

config mr output repaired.mjx

config mjd enable 1
