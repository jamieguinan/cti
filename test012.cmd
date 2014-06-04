new MjpegDemux mjd
new DJpeg dj
new Effects01 ef
new H264 venc
new AAC aenc
new Null null

connect mjd Wav_buffer aenc
connect mjd Jpeg_buffer dj
#connect mjd Jpeg_buffer null

connect dj YUV420P_buffer ef
connect ef YUV420P_buffer venc

config ef rotate 270

config venc preset faster
config venc tune zerolatency
config venc profile baseline
config venc output test012-output.264
# exit the program after 10 seconds at 15fps
config venc max_frames 150
# keyint at 5 seconds
config venc keyint_max 75

config venc cqp 25
#config venc crf 20

config mjd input test012.mjx
config mjd enable 1
