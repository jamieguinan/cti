new MjpegDemux mjd
new DO511 do511
new DJpeg dj
#new ALSAPlayback ap
new SDLstuff sdl

connect mjd O511_buffer do511
connect mjd Jpeg_buffer dj
#connect mjd Wav_buffer ap

connect dj RGB3_buffer sdl
connect do511 RGB3_buffer sdl

config mjd input $INPUT
#config mjd input 127.0.0.1::5000
#config mjd input 127.0.0.1:5000:L

#config ap device hw:0
#config ap rate 44100
#config ap channels 2
#config ap format signed.16-bit.little.endian
#config ap frames_per_io 128

v 0

config mjd enable 1
#config ap enable 1
