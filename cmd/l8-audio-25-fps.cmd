new V4L2Capture vc
new ALSACapture ac
new MjpegMux mjm
new SocketServer ss


connect vc Jpeg_buffer mjm
connect ac Wav_buffer mjm
connect mjm RawData_buffer ss

config ac device hw:0
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

config vc device /dev/video0
config vc format MJPG
config vc size 640x360
config vc fps 25
config vc autoexpose 3

config ss v4port 6666
config ss enable 1

config vc enable 1
config ac enable 1
