# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegLocalBuffer mb
new Null null

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.123:6666

config mjd output frontyard-%Y%m%d-%H%M%S.mjx

connect mjd Wav_buffer null

# Enable mjd to start the whole thing running.
config mjd enable 1

