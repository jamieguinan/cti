# Example usage:
#  MJX=cap-1300291311.mjx ./cti.sh mjxtoh264.cmd

new MjpegDemux mjd
new DJpeg dj
new JpegTran jt
new Y4MOutput y4mout

config mjd input $MJX

config y4mout fps_nom 15
config y4mout fps_denom 1
config y4mout output | x264 /dev/stdin -o output-%s.h264

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj YUV422P_buffer y4mout

config mjd enable 1

