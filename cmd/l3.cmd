new V4L2Capture vc
new DJpeg dj
new CJpeg cj
new Effects01 ef
new MjpegMux mjm

connect vc Jpeg_buffer dj
connect dj RGB3_buffer ef
connect ef RGB3_buffer cj
connect cj Jpeg_buffer mjm

config mjm output 127.0.0.1:5000

config vc device /dev/video2
config vc format MJPG
config vc size 640x480
config vc fps 15
config vc autoexpose 3
config vc exposure 300

config vc enable 1
