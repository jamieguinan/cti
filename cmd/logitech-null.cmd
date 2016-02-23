new V4L2Capture vc
new Null null

# The jpeg/rgb path can be there even if it isn't used.
connect vc Jpeg_buffer null


config vc device UVC
config vc format YUYV
#config vc format MJPG
config vc size 640x480
#config vc size 800x600
config vc fps 30

#config vc autoexpose 3

config vc autoexpose 1
config vc exposure 300

config vc enable 1
system sleep 3
dpt 0
system sleep 3
dpt 0
