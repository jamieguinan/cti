new V4L2Capture vc
new SDLstuff sdl
#new ALSACapture ac
connect vc BGR3_buffer sdl
#connect ac Wav_buffer mjm
config vc device /dev/video0
config vc format BGR3
config vc size 640x480
config vc input Television
#config ac device hw:2
#config ac rate 48000
#config ac channels 2
#config ac format signed.16-bit.little.endian
#config ac enable 1
#config vc mute 0
config vc enable 1
