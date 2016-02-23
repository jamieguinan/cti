#system rm -vf birdcam-*.ts

new MjpegDemux mjd
new DJpeg dj
new Effects01 ef
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
system mkdir -pv /home/guinan/tmp/birdcam
system cp -uv hls-stream.html /home/guinan/tmp/birdcam/stream.html
system cp -uv hls-stream.m3u8 /home/guinan/tmp/birdcam/stream.m3u8
system rm -vf  /home/guinan/tmp/birdcam/birdcam-*.ts
config tsm index_dir /home/guinan/tmp/birdcam
config tsm output /home/guinan/tmp/birdcam/birdcam-%s.ts

#system x.mount /home/guinan/ssh/bluebutton.com
#system mkdir -pv /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdcam
#system cp -uv hls-stream.html /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdc#am/stream.html
#system cp -uv hls-stream.m3u8 /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdc#am/stream.m3u8
#system rm -vf  /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdcam/birdcam-*.ts
#config tsm index_dir /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdcam
#config tsm output /home/guinan/ssh/bluebutton.com/var/www/bluebutton/htdocs/vid/birdcam/birdcam-%s.ts


config tsm duration 5
config tsm pcr_lag_ms 200

connect mjd Wav_buffer aenc
connect mjd Jpeg_buffer dj
connect dj YUV420P_buffer ef
config ef rotate 270
connect ef YUV420P_buffer venc
connect venc H264_buffer tsm
connect aenc AAC_buffer tsm

#config ss v4port 6678
#config ss enable 1

#config mjm output test.mjx
#connect dj Jpeg_buffer mjm

config mjd input 192.168.2.77:6666
config mjd retry 1
config mjd enable 1

#system sleep 60
#config mjd enable 0
#system bash birdcam-production.sh
#exit
