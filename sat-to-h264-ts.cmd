system rm -r outpackets
system rm -vf yuv.fifo
system mkfifo yuv.fifo
system rm -vf pcm.fifo
system mkfifo pcm.fifo
system rm -vf sat1*.ts
system nc sat1w 11301 | mplayer - -really-quiet -noconsolecontrols -vf scale=640:360 -vc ffmpeg2 -vo yuv4mpeg:file=yuv.fifo -ao pcm:file=pcm.fifo &
# system xterm -e 'cat pcm.fifo | pipebench -b 1024 > /dev/null' &

new Y4MInput y4min
new H264 venc
new RawSource pcmin
new AAC aenc
new Null null
#new SocketServer ss

#config ss v4port 6678
#config ss enable 1

config venc output test.264
# faster fast medium
config venc preset faster
#config venc tune psnr
config venc tune zerolatency
#config venc profile baseline
#config venc cqp 25
config venc crf 20
# Assuming 30fps, generate a keyframe every 5 seconds.
config venc keyint_max 150


new MpegTSMux tsm
#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
#connect tsm RawData_buffer ss
config tsm index_dir /home/guinan/tmp/satstream
system mkdir -pv /home/guinan/tmp/satstream
system rm -vf  /home/guinan/tmp/satstream/satstream-*.ts
config tsm output /home/guinan/tmp/satstream/satstream-%s.ts
# config tsm duration 10

#config aenc rate_per_channel 16000

config pcmin input pcm.fifo
#connect pcmin Audio_buffer null
connect pcmin Audio_buffer aenc

config y4min input yuv.fifo
connect y4min YUV420P_buffer venc

connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

config y4min enable 1

#system sleep 60
#exit

