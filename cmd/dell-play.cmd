new MjpegDemux mjd
new DJpeg dj
new ALSAPlayback ap
new SDLstuff sdl
new JpegTran jt
new VSmoother vs
new VFilter vf

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

# Or go direct and save the CPU cycles.
connect mjd Jpeg_buffer dj

connect mjd Wav_buffer ap

#connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl label DELL3000

config sdl mode OVERLAY
#config sdl width 1440
#config sdl height 960

config sdl screen_aspect 1

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
#connect dj RGB3_buffer sdl

connect dj YUV422P_buffer vf
connect vf YUV422P_buffer sdl

#config mjd input ./rcv
# Port 6667 is full speed, port 6668 is every-other.
config mjd input dell3000.local:5103
#config mjd input 192.168.2.109:6668
#config mjd input 127.0.0.1:5000:L


config ap device default
#config ap useplug 1

config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian
config ap frames_per_io 128

config mjd enable 1
config ap enable 1

# Side-channel output, to be toggled at runtime.
config mjd output /tmp/dell-%Y%m%d-%H%M%S.mjx
config mjd rec_key R
