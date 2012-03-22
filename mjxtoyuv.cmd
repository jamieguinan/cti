# Example usage:
#  MJX=cap,mjx ./cti.sh mjxtoy4m.cmd

new MjpegDemux mjd
new DJpeg dj
new JpegTran jt
new Y4MOutput y4mout

config mjd input $MJX

config y4mout fps_nom 25
config y4mout fps_denom 1
config y4mout output 

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj 422P_buffer y4mout

config mjd enable 1

