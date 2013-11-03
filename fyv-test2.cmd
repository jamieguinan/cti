# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegLocalBuffer mb
new Y4MOutput y4mout
new DJpeg dj
# new JpegTran jt
new MotionDetect md
new WavOutput wo
new VFilter vf
new ImageLoader il

new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

include text.cmd
include clock.cmd

# DJpeg setup
config dj dct_method ifast
# config dj dct_method float

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.75:6666

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 25
config y4mout fps_denom 1

system rm -f fifo.y4m
system mkfifo fifo.y4m
system rm -f fifo.wav
system mkfifo fifo.wav

config y4mout output fifo.y4m
config wo output fifo.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example fifo.wav fifo.y4m -A 20 -V 140 -f 25 -F 1  2> /dev/null | tee cap-$(now).ogv |   oggfwd 66.228.32.219 8000 bluebutton0123 /frontyard.ogv &

#system cat fifo.y4m > /dev/null &
#system cat fifo.wav > /dev/null &

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

config mjd output frontyard-%Y%m%d-%H%M%S.mjx

##connect dj RGB3_buffer cc
connect dj 422P_buffer cc
connect dj GRAY_buffer md

##connect cc RGB3_buffer y4mout
connect cc 422P_buffer y4mout

#connect cc RGB3_buffer vf
#connect vf RGB3_buffer y4mout

config md md_threshold 15000

connect md Config_msg cc
connect md MotionDetect_result mb

connect il:GRAY_buffer md:MASK_buffer


config il file fyv-mask.pgm

# Enable mjd to start the whole thing running.
config mjd enable 1

