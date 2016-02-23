new V4L2Capture vc
new CJpeg cj
new ALSACapture ac
new MjpegMux mjm
connect vc BGR3_buffer cj
connect cj Jpeg_buffer mjm
connect ac Wav_buffer mjm
config mjm output cx88-%Y%m%d-%H%M%S.mjx
config vc device cx88
config vc format BGR3
config vc size 640x480
config vc input Television
config ac device hw:3
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian
config ac enable 1
config vc mute 0
config vc enable 1
