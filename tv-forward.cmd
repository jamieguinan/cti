# Pull from aria, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new MjpegDemux mjd
new MjpegLocalBuffer mb
new Y4MOutput y4mout
new DJpeg dj
# new JpegTran jt
new WavOutput wo
new VFilter vf
new NScale ns

# DJpeg setup
config dj dct_method ifast
# config dj dct_method float

# Demuxer setup.
config mjd use_feedback 0
config mjd retry 1
config mjd input 192.168.2.109:6667

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 30000
config y4mout fps_denom 1001

system rm -f fifo.y4m
system mkfifo fifo.y4m
system rm -f fifo.wav
system mkfifo fifo.wav

config y4mout output fifo.y4m
config wo output fifo.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example fifo.wav fifo.y4m -A 20 -V 120 -f 30000 -F 1001 | tee tvcap-$(now).ogv |  oggfwd 66.228.32.219 8000 bluebutton0123 /tv.ogv &

config mjd output tv-%Y%m%d-%H%M%S.mjx

config ns Nx 2
config ns Ny 2

connect mjd Jpeg_buffer dj
connect mjd Wav_buffer wo

# 1:
#connect dj YUV422P_buffer ns
#connect ns YUV422P_buffer y4mout

# 2:
#connect dj YUV422P_buffer y4mout

# 3:
connect dj YUV422P_buffer vf
config vf bottom_crop 240
connect vf YUV422P_buffer y4mout


# Enable mjd to start the whole thing running.
config mjd enable 1

