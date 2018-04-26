system rm -vf /dev/shm/hls1/*.ts
system mkdir -pv /dev/shm/hls1
system cp hls-stream.html /dev/shm/hls1/test.html
system cp hls-stream.m3u8 /dev/shm/hls1/stream.m3u8
	
new V4L2Capture vc
new ALSACapture ac
new ColorSpaceConvert csc
new RPiH264Enc enc
new AAC aenc
new MpegTSMux tsm
new PushQueue pq

connect vc YUV422P_buffer csc
connect csc YUV420P_buffer enc
connect enc H264_buffer tsm
connect ac Wav_buffer aenc
connect aenc AAC_buffer tsm

#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
config tsm index_dir /dev/shm/hls1
config tsm output /dev/shm/hls1/frontporch-%Y%m%d-%H%M%S.ts

config vc device /dev/video0
config vc format YUYV
config vc size 640x360
config vc fps 15
config vc autoexpose 3

config enc fps 15
config enc bitrate 300000

# These 2 need to be the same.
# FIXME: Could share a config key somehow?
config enc gop_seconds 15
config tsm duration 15

config ac device U0x46d0x81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
config ac frames_per_io 256

config pq uri http://nucf696.local/cgi-bin/ws/postdata
config pq service_key /home/pi/etc/hls_service_key.nucf696

connect tsm Push_data pq

config vc enable 1
config ac enable 1

ignoreeof 1
