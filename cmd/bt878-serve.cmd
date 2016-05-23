new V4L2Capture vc
new CJpeg cj
new MjpegMux mjm
new SocketServer ss

connect vc BGR3_buffer cj
connect cj Jpeg_buffer mjm
connect mjm RawData_buffer ss

config ss v4port 5103
config ss enable 1

config vc device BT878
config vc format BGR3
config vc std NTSC
config vc size 640x480
config vc input Composite1
config vc mute 0
#config vc Contrast 63
config vc enable 1
