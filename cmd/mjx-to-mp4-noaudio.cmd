new MjpegDemux mjd
new Y4MOutput y4mout
new DJpeg dj

# DJpeg setup
config dj dct_method ifast
# config dj dct_method float

# Demuxer setup.
config mjd use_feedback 0
config mjd eof_notify 1
config mjd input %input

# Parameters.  Only handles fixed FPS, and I have to make sure the source
# is set up for the same FPS, but it works.
config y4mout fps_nom 30000
config y4mout fps_denom 1001

# FIXME: Use unique name (PID for example) so can run multiple instances.
system rm -f /tmp/xx.y4m
system mkfifo /tmp/xx.y4m

# Try -b:v 2000k for better quality.
system xterm -hold -e ffmpeg -r 29.97 -i /tmp/xx.y4m -codec:v h264 -y output.mp4 &
# subproc p0 ffmpeg...

config y4mout output /tmp/xx.y4m

connect mjd Jpeg_buffer dj
connect dj YUV420P_buffer y4mout

# Enable mjd to start the whole thing running.
config mjd enable 1

# waiton p0
# exit 0
