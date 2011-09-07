new GdkCapture gc
new CJpeg cj
new MjpegMux mjm

config gc filename $FILENAME
config gc idle_quit_threshold 300
config mjm output /tmp/cap-%Y%m%d-%H%M%S.mjx

connect cj Jpeg_buffer mjm
connect gc RGB3_buffer cj
