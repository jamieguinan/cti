new MjpegDemux mjd
new DJpeg dj
#new ALSAPlayback ap
new SDLstuff sdl
new JpegTran jt
new VSmoother vs

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj

# Or go direct and save the CPU cycles.
#connect mjd Jpeg_buffer dj

#connect mjd Wav_buffer ap

connect sdl Feedback_buffer mjd

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
connect dj RGB3_buffer sdl

config jt transform ROT_90
config jt crop 600x400

config mjd input 127.0.0.1:6666

#config ap device plughw:0
#config ap rate 16000
#config ap channels 1
#config ap format signed.16-bit.little.endian
#config ap frames_per_io 128

config mjd enable 1
config ap enable 1
