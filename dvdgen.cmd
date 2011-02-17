#new VFilter vf
#new AudioLimiter al
new MjpegDemux mjd
new DJpeg dj
new Mpeg2Enc mve
new Mp2Enc mae

config mjd input $MJX
config mjd use_feedback 2
config mve vout temp.m2v
config mae aout temp.mp2
#config al limit 0  
#config vf linear_blend 1

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer mae

#connect al Wav_buffer mae

connect dj 422P_buffer mve

#connect dj 422P_buffer vf
#connect vf 422P_buffer mve

connect mve Feedback_buffer mjd

config mjd enable 1
