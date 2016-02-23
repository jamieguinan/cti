new MjpegDemux mjd
new MjpegMux mjm
new JpegTran jt

# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer mjm

config mjd use_feedback 0
config mjd input 127.0.0.1:6666
config mjm output |ffmpeg2theora --inputfps 15 /dev/stdin -V 100 -f mjpeg -F 15 -o /dev/stdout | tee out.ogv | oggfwd localhost 8000 hackme /frontyard.ogv

# --inputfps 15

config mjd enable 1
