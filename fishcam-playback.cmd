new MjpegDemux mjd
new DJpeg dj
# new JpegTran jt
new SDLstuff sdl
# new WavOutput wo
new Null null

# DJpeg setup
config dj dct_method ifast

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.123:6667

connect mjd Jpeg_buffer dj
connect dj RGB3_buffer sdl

connect mjd Wav_buffer null

config mjd enable 1
