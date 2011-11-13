v 3
new GdkCapture gc
new CJpeg cj
new MjpegMux mjm
new VFilter vf

config vf bottom_crop 70
config gc filename $FILENAME
config gc idle_quit_threshold 300
config mjm output /tmp/cap-%Y%m%d-%H%M%S.mjx

config cj quality 95


connect cj Jpeg_buffer mjm
connect gc RGB3_buffer vf
connect vf RGB3_buffer cj



