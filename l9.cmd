new V4L2Capture vc
new MjpegMux mjm
new SocketServer ss
new Null null

connect vc Jpeg_buffer mjm
#connect vc Jpeg_buffer null
#connect mjm RawData_buffer ss
connect mjm RawData_buffer null

config vc device UVC
config vc format MJPG
config vc size 640x360
config vc fps 25
config vc autoexpose 3

config ss v4port 6666
config ss enable 1

config vc enable 1

