new MjpegDemux mjd
new DJpeg dj
new WavOutput wo
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

connect mjd Wav_buffer wo

#connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl label DELL3000

config sdl mode OVERLAY
#config sdl width 1440
#config sdl height 960

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
#connect dj RGB3_buffer sdl

connect dj YUV422P_buffer vf
connect vf YUV422P_buffer sdl

#config mjd input ./rcv
# Port 6667 is full speed, port 6668 is every-other.
config mjd input dell3000:5103
#config mjd input 192.168.2.109:6668
#config mjd input 127.0.0.1:5000:L


config wo output output.wav

config mjd enable 1

