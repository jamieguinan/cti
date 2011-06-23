new V4L2Capture vc
new SDLstuff sdl
new ALSACapture ac
new ALSAPlayback ap

connect vc BGR3_buffer sdl
config sdl mode GL

connect ac Wav_buffer ap

config ac device hw:2
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian
config ac enable 1

config ap device hw:0
config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian
config ap enable 1

config vc device cx88
config vc format BGR3
config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

#system sleep 3
#Zoom SDL viewport, this actually works!
#config sdl width 1280
#config sdl height 960
