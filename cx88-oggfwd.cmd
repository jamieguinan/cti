# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new V4L2Capture vc
new Y4MOutput y4mout
new DJpeg dj
new JpegTran jt
new MotionDetect md

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 29.97
config y4mout fps_denom 1
config y4mout output |ffmpeg2theora - -V 300 --soft-target -o /dev/stdout 2> /dev/null | pipebench -b 8000 | oggfwd localhost 8000 hackme /tv

config vc device /dev/video0
config vc format MJPG
config vc format BGR3
config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

# Connect everything up
connect vc BGR3_buffer y4mout

# Enable mjd to start the whole thing running.
config vc enable 1
