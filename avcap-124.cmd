new MjpegDemux mjd
new MjpegMux mjm
new JpegTran jt

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer mjm

config mjd use_feedback 0
config mjd input 192.168.2.124:6666
config mjm output cap-%Y%m%d-%H%M%S.mjx

config mjd enable 1
