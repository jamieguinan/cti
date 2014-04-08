# Example usage:
#  MJX=cap-1300291311.mjx ./cti.sh mjxtoh264.cmd

new MjpegDemux mjd
new DJpeg dj
new Effects01 ef
new JpegTran jt
new Y4MOutput y4mout

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj YUV420P_buffer ef
connect ef YUV420P_buffer y4mout

config ef rotate 270

config y4mout fps_nom 15
config y4mout fps_denom 1
config y4mout output | x264 /dev/stdin -o output-%s.h264

config mjd input 192.168.2.77:6666
config mjd retry 1
config mjd enable 1

system sleep 60
exit
