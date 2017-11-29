new Y4MInput yi
new H264 venc
new MpegTSMux tsm

connect yi YUV420P_buffer venc
connect venc H264_buffer tsm

config venc output /tmp/test.h264
config venc preset faster
#config venc tune psnr
config venc tune zerolatency
#config venc profile baseline
config venc cqp 25
#config venc crf 20
# Assuming 15fps, generate a keyframe every 5 seconds.
config venc keyint_max 75

config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
config tsm index_dir /tmp
config tsm output /tmp/test-%s.ts
config tsm duration 5
config tsm pcr_lag_ms 200


config yi input /tmp/test.y4m
config yi enable 1
config yi exit_on_eof 0

