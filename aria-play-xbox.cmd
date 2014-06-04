new MjpegDemux mjd
new DJpeg dj
new ALSAPlayback ap
new SDLstuff sdl
new JpegTran jt
#new VSmoother vs
new VFilter vf

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

# Or go direct and save the CPU cycles.
connect mjd Jpeg_buffer dj

connect mjd Wav_buffer ap

connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl mode OVERLAY

config sdl width 852
config sdl height 480

config sdl label XBox

connect dj YUV422P_buffer vf
connect vf YUV422P_buffer sdl

#connect dj YUV422P_buffer sdl

#config mjd input ./rcv
config mjd input 192.168.2.22:6667
#config mjd input 127.0.0.1:5000:L

config ap device plughw:2
config ap rate 8000
#config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian
config ap frames_per_io 128

config mjd enable 1
config ap enable 1

#config vf horizontal_filter -1,2,0,2,-1
#config vf horizontal_filter -1,2,4,2,-1
#config vf horizontal_filter -1,3,1,3,-1
#config vf horizontal_filter 3,2,1,2,3
#config vf horizontal_filter 0,0,1,0,0

