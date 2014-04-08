new MjpegDemux mjx
new DJpeg dj
new H264 venc
new AAC aenc
new MpegTSMux tsm

connect mjd Wav_buffer aenc
connect mjd Jpeg_buffer dj
connect dj YUV420P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
config tsm output test010-%s.ts
config tsm duration 10

config venc output test010.264
config venc preset ultrafast
config venc tune psnr
config venc profile baseline
config venc output test010-output.264

config mjd input frontyard-20120716-112614.mjx
config mjd enable 1
