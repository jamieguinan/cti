# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new Y4MOutput y4mout
new DJpeg dj
new JpegTran jt
new MotionDetect md

# Demuxer setup.
config mjd use_feedback 0
config mjd input 192.168.2.75:6666

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 25
config y4mout fps_denom 1
config y4mout output | ffmpeg2theora - -V 180 -o /dev/stdout 2> /dev/null | tee cap.ogv | oggfwd localhost 8000 hackme /frontyard.ogv

# Connect everything up
#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj
connect mjd Jpeg_buffer dj
connect dj YUV422P_buffer y4mout

# Enable mjd to start the whole thing running.
config mjd enable 1
