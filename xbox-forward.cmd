# Pull from arias, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new Y4MOutput y4mout
new DJpeg dj
new WavOutput wo

# DJpeg setup
config dj dct_method ifast
config dj every 6
# config dj dct_method float

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.22:6667

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 5
config y4mout fps_denom 1

system rm -f xbox.y4m
system mkfifo xbox.y4m
system rm -f xbox.wav
system mkfifo xbox.wav

config y4mout output xbox.y4m
config wo output xbox.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example xbox.wav xbox.y4m -A 20 -V 140 -f 5 -F 1  2> /dev/null | tee xbox-$(now).ogv |   oggfwd 66.228.32.219 8000 bluebutton0123 /xbox.ogv &
#system ../libtheora-1.2.0alpha1/examples/encoder_example xbox.y4m  -V 140 -f 5 -F 1  2> /dev/null | tee xbox-$(now).ogv |   oggfwd 66.228.32.219 8000 bluebutton0123 /xbox.ogv &

#system cat xbox.y4m > /dev/null &
#system cat xbox.wav > /dev/null &

# "-v" values work out to roughly (value * 25
# -v 3
# -A 20 -V 160
# Additional encoder_example options to try:
#  -k 2  [ or more ]
#  -d 25  [ or more ]
#  --soft-target

# Connect everything up

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer wo

config mjd output xbox-%Y%m%d-%H%M%S.mjx

connect dj YUV422P_buffer  y4mout

# Enable mjd to start the whole thing running.
config mjd enable 1

