new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl
new ALSAPlayback ap
new VSmoother vs

new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

include clock.cmd

connect mjd Wav_buffer ap
connect mjd Jpeg_buffer dj

connect sdl Feedback_buffer mjd

# 'q' to quit.
connect sdl:Keycode_msg sdl:Keycode_msg
# 's' for snapshot
connect sdl:Keycode_msg_2 mjd:Keycode_msg

#connect dj RGB3_buffer sdl
connect dj YUV422P_buffer sdl

#connect dj YUV422P_buffer cc
#connect cc YUV422P_buffer sdl


# ifast works best on sully.
config dj dct_method ifast

#config ap useplug 1
#config ap device plughw:0
#config ap device Dummy
#config ap device UA25
#config ap device CK804
#config ap device ua_dmix_plug
config ap device default
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

# Side-channel output, to be toggled at runtime.
config mjd output /av/media/tmp/frontyard-%Y%m%d-%H%M%S.mjx

config sdl label FRONTYARD

# Use relay, see "frontyard-relay.cmd"
# config mjd input 127.0.0.1:7131

config mjd input 192.168.2.75:6666

config mjd retry 1
config mjd enable 1

config mjd rec_key R

config ap enable 1

ignoreeof 1
