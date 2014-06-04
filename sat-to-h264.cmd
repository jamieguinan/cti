system rm -vf yuv.fifo
system mkfifo yuv.fifo
system nc sat1w 11301 | mplayer -  -noconsolecontrols -really-quiet -vc ffmpeg2 -vo yuv4mpeg:file=yuv.fifo -ao &
new Y4MInput y4min
new Null null
new H264 venc
config y4min input yuv.fifo
config y4min enable 1
connect y4min YUV420P_buffer venc
config venc output sat.264
