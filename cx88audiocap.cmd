new V4L2Capture vc
new ALSACapture ac
new MjpegMux mjm

connect ac Wav_buffer mjm

config mjm output cx88-audio-%Y%m%d-%H%M%S.mjx

config ac device hw:2
#config ac device hw:0
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian
config ac enable 1

config vc device cx88
config vc format BGR3
config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

#system sleep 600
#exit
