# Post-processing

new MjpegDemux mjd
new MjpegMux mjm
new JpegTran jt

config mjm output demux-%Y%m%d-%H%M%S.mjx
config mjd input argv1
config jt transform ROT_90
config jt crop 640x480

connect mjd Wav_buffer mjm
connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer mjm
