new V4L2Capture vc
new MjpegMux mjm
new SocketServer ss
new ALSACapture ac
new Null null

connect vc Jpeg_buffer mjm
connect ac Wav_buffer mjm
connect mjm RawData_buffer ss
#connect mjm RawData_buffer null
config ss max_total_buffered_data 100000

config vc device /dev/video1
config vc format MJPG
config vc LED1.Mode 0
config vc size 800x600
config vc fps 20
config vc enable 1
config vc autoexpose 1
config vc exposure 300

config ac device hw:3
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
config ac enable 1

config ss v4port 6666
config ss enable 1

config vc enable 1
#v 268435456
