new MjpegDemux mjd
new WavOutput wo

# Demuxer setup.
config mjd use_feedback 0
config mjd eof_notify 1
config mjd input %input

config wo output output.wav

connect mjd Wav_buffer wo

# Enable mjd to start the whole thing running.
config mjd enable 1
