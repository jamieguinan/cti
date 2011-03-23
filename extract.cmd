new MjpegDemux mjd
new JpegTran jt
new JpegFiler jf

config mjd input $MJX
config mjd use_feedback 0

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer jf
