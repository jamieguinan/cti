new MjpegDemux mjd
new Y4MOutput y4mout
new JpegTran jt
new DJpeg dj

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj 422P_buffer y4mout

config mjd use_feedback 0
config mjd input 127.0.0.1:6666
config y4mout fps_nom 15
config y4mout fps_denom 1
config y4mout output |ffmpeg2theora - -V 150  -o /dev/stdout | oggfwd localhost 8000 hackme /frontyard2.ogv
#config y4mout output out-%s.y4m

config mjd enable 1
