# Pull from lr1, feed yCrCb to theora encoder, then to oggfwd.

new MjpegDemux mjd
new MjpegStreamBuffer msb
new DJpeg dj
new MotionDetect md
new Y4MOutput y4mout
new WavOutput wo
new VFilter vf

new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

include clock.cmd
include text.cmd

# DJpeg setup
config dj dct_method islow

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.75:6666

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 25
config y4mout fps_denom 1

system rm -f fyv-stream-save-forward.y4m
system mkfyv-stream-save-forward fyv-stream-save-forward.y4m
system rm -f fyv-stream-save-forward.wav
system mkfyv-stream-save-forward fyv-stream-save-forward.wav

config y4mout output fyv-stream-save-forward.y4m
config wo output fyv-stream-save-forward.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example fyv-stream-save-forward.wav fyv-stream-save-forward.y4m -A 20 -V 140 -f 25 -F 1  2> /dev/null | tee cap-$(now).ogv |  oggfwd localhost 8000 hackme /frontyard.ogv &

#system cat fyv-stream-save-forward.y4m > /dev/null &
#system cat fyv-stream-save-forward.wav > /dev/null &

# "-v" values work out to roughly (value * 25
# -v 3
# -A 20 -V 160
# Additional encoder_example options to try:
#  -k 2  [ or more ]
#  -d 25  [ or more ]
#  --soft-target

# Connect everything up

connect mjd RawData_buffer mb

#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer wo

##connect dj RGB3_buffer cc
connect dj YUV422P_buffer cc
connect dj GRAY_buffer md

##connect cc RGB3_buffer y4mout
connect cc YUV422P_buffer y4mout

connect md Config_msg cc

# Enable mjd to start the whole thing running.
config mjd enable 1

# mt
