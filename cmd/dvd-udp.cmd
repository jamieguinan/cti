new Null null
new Y4MInput y
new AAC aenc
new RawSource a
new H264 venc
new MpegTSMux tsm
new UDPTransmit ut

connect a Audio_buffer aenc
connect y YUV420P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm
connect tsm RawData_buffer ut

config venc preset faster
config venc tune zerolatency
config venc cqp 25
#config venc crf 20
# divide keyint_max by nominal frame rate for duration below
config venc keyint_max 15

config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258
config tsm duration 1
config tsm pcr_lag_ms 200

config ut port 6681
config ut addr 127.0.0.1
#config ut addr 192.168.1.15
config ut buffer_level 1316

# 
config y input /tmp/v
config y enable 1
config a input /tmp/a

# Increase max pending messages
mpm 500
