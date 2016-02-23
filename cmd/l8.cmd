new V4L2Capture vc
new MjpegMux mjm
new SocketServer ss
#new ALSACapture ac

#connect ac Wav_buffer mjm
connect vc Jpeg_buffer mjm
connect mjm RawData_buffer ss

#config ac device hw:3
#config ac rate 48000
#config ac channels 2
#config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
#config ac frames_per_io 64

config vc device /dev/video1
config vc format MJPG
config vc size 640x400
config vc fps 15
config vc autoexpose 3

config ss v4port 6666
config ss enable 1

config vc enable 1
