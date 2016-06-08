new V4L2Capture vc
new RPiH264Enc enc
new MpegTSMux tsm

connect vc YUV422P_buffer enc
connect enc H264_buffer tsm

#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258

config tsm index_dir /tmp
config tsm output /tmp/test%s.ts

config vc device /dev/video0
config vc format YUYV
config vc size 640x360
config vc fps 15
config vc autoexpose 3

config vc enable 1

