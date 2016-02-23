new MjpegDemux mjd
new JpegTran jt
new Null null

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer null

v 1

config mjd use_feedback 0
config mjd input sample-broken.mjx
config mjd enable 1
