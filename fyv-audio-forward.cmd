# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegLocalBuffer mb
new WavOutput wo
new Null null

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.123:6666

system rm -f fifo.wav
system mkfifo fifo.wav

config wo output fifo.wav

system cat fifo.wav | oggenc - -o /dev/stdout 2> /dev/null | pipebench -b8192 | oggfwd 66.228.32.219 8000 bluebutton0123 /frontyardaudio.ogv &

connect mjd Wav_buffer wo
connect mjd Jpeg_buffer null

config mjd output frontyard-audio-%Y%m%d-%H%M%S.mjx

# Enable mjd to start the whole thing running.
config mjd enable 1

