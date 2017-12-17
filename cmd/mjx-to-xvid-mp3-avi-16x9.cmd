new MjpegDemux mjd
new Y4MOutput y4mout
new DJpeg dj
new WavOutput wo
new VFilter vf

# DJpeg setup
config dj dct_method ifast
# config dj dct_method float

# Demuxer setup.
config mjd use_feedback 0
config mjd eof_notify 1
config mjd input %input

# Parameters.  Only handles fixed FPS, and I have to make sure the source
# is set up for the same FPS, but it works.
config y4mout fps_nom 30000
config y4mout fps_denom 1001

# FIXME: Use unique name (PID for example) so can run multiple instances.
#system rm -f /tmp/xx.y4m
#system rm -f /tmp/xx.wav

# For now, will unlink work?  The idea is that the file
# will persist as long as processes hold a reference
# to it.
system unlink /tmp/xx.y4m
system unlink /tmp/xx.wav

system mkfifo /tmp/xx.y4m
system mkfifo /tmp/xx.wav

config y4mout output /tmp/xx.y4m
config wo output /tmp/xx.wav

# Try -b:v 2000k for better quality.
system ffmpeg -nostdin -r 29.97 -i /tmp/xx.y4m -i /tmp/xx.wav -codec:v xvid -b:v 1500k -codec:a mp3 -y output.avi &
# -b:v 2000k
# -filter:v yadif 
# subproc p0 ffmpeg...

connect mjd Jpeg_buffer dj
connect dj 422P_buffer vf

# The em2820 doesn't quite align things evenly.
config vf top_crop 64
config vf bottom_crop 56

# Filters...
# config vf horizontal_filter -1,2,0,2,-1
#config vf horizontal_filter 0,1,1,1,0
config vf linear_blend 1

connect vf 422P_buffer y4mout
connect mjd Wav_buffer wo

# Enable mjd to start the whole thing running.
config mjd enable 1

# waiton p0
# exit 0
