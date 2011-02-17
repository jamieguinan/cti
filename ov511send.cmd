new V4L2Capture vc
new MjpegMux mjm
#new ALSACapture ac

connect vc O511_buffer mjm
#connect ac Wav_buffer mjm

config mjm output 192.168.2.20:5000

#config ac device hw:0
#config ac rate 48000
#config ac channels 2
#config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
#config ac frames_per_io 64

config vc device /dev/video1
config vc format O511
config vc size 640x480

config vc enable 1
#config ac enable 1
