new V4L2Capture vc
new MjpegMux mjm
new SocketServer ss

connect vc O511_buffer mjm
connect mjm RawData_buffer ss

config vc device gspca
config vc format O511
config vc size 640x480
#config vc fps 15
# config vc autoexpose 3

config ss v4port 6667
config ss enable 1

config vc enable 1
