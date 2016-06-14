new V4L2Capture vc
new ALSACapture ac
new H264 venc
new AAC aenc
new MpegTSMux tsm

config vc device UVC
config vc format YUYV
config vc size 640x360
config vc fps 15

config ac device U0x46d0x81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

# config venc output test.264
# faster fast medium
config venc preset faster
#config venc tune psnr
config venc tune zerolatency
#config venc profile baseline
config venc cqp 25
#config venc crf 20
# Assuming 15fps, generate a keyframe every 5 seconds.
config venc keyint_max 75

#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
config tsm duration 5
config tsm pcr_lag_ms 200
system cp -uv ../hls-stream.html /var/www/html/test/stream.html
system cp -uv ../hls-stream.m3u8 /var/www/html/test/stream.m3u8
system rm -vf /var/www/html/test/webcam-%s.ts
config tsm index_dir /var/www/html/test
config tsm output /var/www/html/test/webcam-%s.ts

connect ac Wav_buffer aenc
connect vc YUV422P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

config vc enable 1
config ac enable 1
