new MjpegDemux mjd
new DJpeg dj
# new JpegTran jt
new SDLstuff sdl
# new WavOutput wo
new Null null

# DJpeg setup
config dj dct_method ifast

# 'q' to quit.
# 's' for snapshot
# 'r' toggles recording
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

# Side-channel output, to be toggled at runtime.
config mjd output /av/media/tmp/fishcam-%Y%m%d-%H%M%S.mjx
config mjd rec_key R

config sdl label FISHCAM

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.75:6667


connect mjd Jpeg_buffer dj
connect dj RGB3_buffer sdl

connect mjd Wav_buffer null

config mjd enable 1
