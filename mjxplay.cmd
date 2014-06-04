new MjpegDemux mjd
new DO511 do511
new DJpeg dj
new SDLstuff sdl
new JpegTran jt
#new VSmoother vs
new ALSAPlayback ap

connect mjd O511_buffer do511

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

# Or go direct and save the CPU cycles.
connect mjd Jpeg_buffer dj

connect mjd Wav_buffer ap

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl

connect dj RGB3_buffer sdl
#connect do511 RGB3_buffer sdl

#connect sdl Feedback_buffer mjd
connect ap Feedback_buffer mjd

connect sdl Config_msg mjd

config mjd input $MJX
config mjd use_feedback 1

config ap device plughw:0
system sleep 1
config ap rate 32000
system sleep 1
config ap channels 2
config ap format signed.16-bit.little.endian
config ap frames_per_io 128
config ap enable 1
config mjd enable 1

