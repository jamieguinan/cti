# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new Y4MOutput y4mout
new DJpeg dj
# new JpegTran jt
new MotionDetect md
new WavOutput wo

new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

include clock.cmd

# Draw text underlay.
config cc command set_source_rgb 0.2 0.2 0.2
config cc command identity_matrix
config cc command move_to 10.0 0.0
config cc command rel_line_to 80.0 0.0
config cc command rel_line_to 0.0 15.0
config cc command rel_line_to -80.0 0.0
config cc command close_path
config cc command fill

# Draw text.
config cc command set_source_rgb 0.9 0.9 0.9
config cc command identity_matrix
config cc command move_to 10.0 10.0
config cc command show_text


# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.123:6666

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

system ../libtheora-1.2.0alpha1/examples/encoder_example fifo.wav fifo.y4m -A 20 -v 3 -f 25 -F 1 | tee cap.ogv | oggfwd localhost 8000 hackme /frontyard.ogv &

# -A 20 -V 160
# Additional encoder_example options to try:
#  -k 2  [ or more ]
#  -d 25  [ or more ]
#  --soft-target


# Connect everything up

#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer wo

connect dj RGB3_buffer cc
connect dj GRAY_buffer md
connect cc RGB3_buffer y4mout
connect md Config_msg cc

# Enable mjd to start the whole thing running.
config mjd enable 1
