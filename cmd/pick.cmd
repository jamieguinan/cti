new MjpegDemux mjd
new JpegFiler jf
new JpegTran jt

config mjd input block.bin

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer jf


config mjd enable 1
