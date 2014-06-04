# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegMux mjm

config mjd input 192.168.2.75:6666

config mjm output /av/media/tmp/fyv-%Y%m%d-%H%M%S.mjx

connect mjd Jpeg_buffer mjm
connect mjd Wav_buffer mjm

# Enable mjd to start the whole thing running.
config mjd enable 1

