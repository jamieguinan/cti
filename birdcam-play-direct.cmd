new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl
new ALSAPlayback ap
#new JpegTran jt
new Effects01 ef

connect mjd Wav_buffer ap
connect mjd Jpeg_buffer dj

connect sdl Feedback_buffer mjd

# 'q' to quit.
connect sdl:Keycode_msg sdl:Keycode_msg
# 's' for snapshot
connect sdl:Keycode_msg_2 mjd:Keycode_msg

connect dj YUV420P_buffer ef
connect ef YUV420P_buffer sdl

#connect dj RGB3_buffer sdl
#connect dj 422P_buffer sdl

config ef rotate 270

#config ap device plughw:0
config ap device default
#config ap device Dummy
config ap useplug 1
#config ap device CK804
config ap rate 16000
config ap channels 1
config ap format signed.16-bit.little.endian
config ap frames_per_io 128

config sdl mode OVERLAY
#config sdl mode GL
#config sdl mode SOFTWARE
#config sdl width 640
#config sdl height 360

#config sdl width 1280
#config sdl height 720

config sdl label BIRDCAM

config mjd input 192.168.2.122:6666
config mjd retry 1
config mjd enable 1

config ap enable 1

ignoreeof 1
