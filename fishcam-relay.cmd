# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegLocalBuffer mb
new Y4MOutput y4mout
new DJpeg dj
# new JpegTran jt
new WavOutput wo
new VFilter vf
new ImageLoader il

include text.cmd

# DJpeg setup
config dj dct_method ifast

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.75:6667

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 15
config y4mout fps_denom 1

system rm -f fishcam.y4m
system mkfifo fishcam.y4m
system rm -f fishcam.wav
system mkfifo fishcam.wav

config y4mout output fishcam.y4m
config wo output fishcam.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example fishcam.wav fishcam.y4m -A 20 -V 140 -f 15 -F 1  2> /dev/null | tee cap-$(now).ogv | oggfwd 66.228.32.219 8000 bluebutton0123 /fishcam.ogv &

# | pipebench -b 1024 

#system cat fishcam.y4m > /dev/null &
#system cat fishcam.wav > /dev/null &

# "-v" values work out to roughly (value * 25
# -v 3
# -A 20 -V 160
# Additional encoder_example options to try:
#  -k 2  [ or more ]
#  -d 25  [ or more ]
#  --soft-target

# config vf trim 4

# Connect everything up

# connect mjd RawData_buffer mb

#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer wo

connect dj 422P_buffer y4mout

config mjd output fishcam-%Y%m%d-%H%M%S.mjx

# Enable mjd to start the whole thing running.
config mjd enable 1

