new CJpeg cj
new MjpegMux mjm
new V4L2Capture vc
new ALSACapture ac

system amixer sset Line 0%,0%
system amixer sset Mic 25%,25% cap
system amixer sset Capture 25%,25% cap
system amixer sset 'Mic Boost (+20dB)' off

connect vc 422P_buffer cj
connect cj Jpeg_buffer mjm
connect ac Wav_buffer mjm

config mjm output sully:4555

config cj quality 85

config ac device hw:0
config ac rate 32000
config ac channels 2
config ac format signed.16-bit.little.endian

config vc device /dev/video0
config vc mute 0
config vc format 422P
config vc std NTSC
config vc input Composite1
config vc size 704x480
config vc brightness 30000
config vc contrast 32000
config vc saturation 32000

config vc enable 1
config ac enable 1
