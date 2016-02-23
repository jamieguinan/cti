new Y4MInput y4min
new H264 venc
new AAC aenc

config venc output test.264

new MpegTSMux tsm
#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258

#connect tsm RawData_buffer ss

#config tsm output test.ts
config tsm output sat%s.ts
config tsm duration 10


connect mjd Wav_buffer aenc
#connect mjd Jpeg_buffer dj
connect mjd Jpeg_buffer dj
connect dj YUV420P_buffer ef
config ef rotate 270
connect ef YUV420P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

#config ss v4port 6678
#config ss enable 1

config mjm output test.mjx
connect dj Jpeg_buffer mjm

config mjd input 192.168.2.77:6666
config mjd retry 1
config mjd enable 1

system sleep 60
exit
