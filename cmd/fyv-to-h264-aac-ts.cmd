new MjpegDemux mjd
new DJpeg dj
new H264 venc
new AAC aenc
#new SocketServer ss
#new MjpegMux mjm

config venc output test.264
# faster fast medium
config venc preset faster
#config venc tune psnr
config venc tune zerolatency
#config venc profile baseline
config venc cqp 25
#config venc crf 20
# Assuming 15fps, generate a keyframe every 5 seconds.
config venc keyint_max 75

new MpegTSMux tsm
#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258

#connect tsm RawData_buffer ss
system mkdir -pv /home/guinan/tmp/fyv
system rm -vf  /home/guinan/tmp/fyv/fyv-*.ts
config tsm index_dir /home/guinan/tmp/fyv
config tsm output /home/guinan/tmp/fyv/fyv-%s.ts
config tsm duration 5
config tsm pcr_lag_ms 500

connect mjd Wav_buffer aenc
#connect mjd Jpeg_buffer dj
connect mjd Jpeg_buffer dj
connect dj YUV420P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

#config ss v4port 6678
#config ss enable 1

#config mjm output test.mjx
#connect dj Jpeg_buffer mjm

config mjd input 192.168.2.75:6666
config mjd retry 1
config mjd enable 1
