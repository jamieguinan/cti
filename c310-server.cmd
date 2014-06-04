new V4L2Capture vc
new MjpegMux mjm
new SocketServer ss

connect vc Jpeg_buffer mjm
connect mjm RawData_buffer ss

config ss v4port 6666
config ss enable 1

config vc device /dev/video1
config vc format MJPG
config vc size 640x480
config vc fps 15
config vc autoexpose 3

config vc enable 1
