new MjpegDemux mjd
new MjpegMux mjm
new Y4MOutput y4mout
new JpegTran jt
new DJpeg dj

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer mjm

config mjd use_feedback 0
config mjd input 127.0.0.1:6666
config mjm output |ffmpeg2theora - -V 150 -f mjpeg -o /dev/stdout | oggfwd localhost 8000 hackme /frontyard2.ogv

config mjd enable 1
