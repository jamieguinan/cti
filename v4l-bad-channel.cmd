new V4L2Capture vc
new Null null

connect vc RGB3_buffer null

config vc device /dev/video0
config vc format BGR3
config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

config vc frequency 121250000

