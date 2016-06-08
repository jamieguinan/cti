new V4L2Capture vc
new Y4MOutput yo

connect vc YUV422P_buffer yo

config yo raw 1
config yo reduce 1
config yo output /dev/shm/y4m.raw
#config yo output /dev/null
#config yo output /tmp/y4m.fifo

config vc device /dev/video0
config vc format YUYV
config vc size 640x360
config vc fps 15
config vc autoexpose 3

config vc enable 1

