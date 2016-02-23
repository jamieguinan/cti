new V4L2Capture vc
new MjpegMux mjm

connect vc Jpeg_buffer mjm

config mjm output /tmp/cap.mjx

config vc device /dev/video0
config vc format MJPG
config vc size 640x480
config vc fps 15
config vc autoexpose 3
config vc exposure 300

config vc enable 1
