new V4L2Capture vc
new ALSACapture ac
new MjpegMux mjm
new SocketServer ss
# Single-serve server
new SocketServer sss
new ResourceMonitor rsm
new XMLMessageServer ms

connect vc Jpeg_buffer mjm
connect ac Wav_buffer mjm
connect mjm RawData_buffer ss

#config rsm rss_limit 10000

config ac device 81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

config vc device 81b
config vc format MJPG
config vc size 640x360
config vc fps 25
config vc autoexpose 3

config ss v4port 6666
config ss enable 1

config sss v4port 6670
config sss enable 1

config ms v4port 6669
config ms enable 1

config vc enable 1
config ac enable 1
ignoreeof
