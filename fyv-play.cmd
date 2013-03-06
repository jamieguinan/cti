new MjpegDemux mjd
new DJpeg dj
new SDLstuff sdl
new ALSAPlayback ap

connect mjd Wav_buffer ap
connect mjd Jpeg_buffer dj

connect sdl Feedback_buffer mjd

#connect dj RGB3_buffer sdl
connect dj 422P_buffer sdl

#config ap device plughw:0
config ap device dummy
config ap rate 16000
config ap channels 1
config ap format signed.16-bit.little.endian
config ap frames_per_io 128

config sdl mode OVERLAY
#config sdl mode GL
#config sdl mode SOFTWARE

config mjd input 192.168.2.123:6666
config mjd retry 1
config mjd enable 1

config ap enable 1
