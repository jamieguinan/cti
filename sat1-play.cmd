new MjpegDemux mjd
new DJpeg dj
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

connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl mode OVERLAY
#config sdl width 1440
#config sdl height 960

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
#connect dj RGB3_buffer sdl
connect dj 422P_buffer vf
connect vf 422P_buffer sdl

config mjd input 192.168.2.74:6667

config mjd enable 1
