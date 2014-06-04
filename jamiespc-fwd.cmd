new V4L2Capture vc
new Y4MOutput y4mout
new ALSACapture ac
new WavOutput wo
#new DJpeg dj
#new JpegTran jt
#new MotionDetect md

config ac device U0x46d0x81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

#new CairoContext cc

# Basic cairo setup.
#config cc width 100
#config cc height 100
#config cc command set_line_width 2.0

#include clock.cmd

# Draw text underlay.
#config cc command set_source_rgb 0.2 0.2 0.2
#config cc command identity_matrix
#config cc command move_to 10.0 0.0
#config cc command rel_line_to 80.0 0.0
#config cc command rel_line_to 0.0 15.0
#config cc command rel_line_to -80.0 0.0
#config cc command close_path
#config cc command fill

# Draw text.
#config cc command set_source_rgb 0.9 0.9 0.9
#config cc command identity_matrix
#config cc command move_to 10.0 10.0
#config cc command show_text

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
#config y4mout fps_nom 15
#config y4mout fps_denom 1

system rm -f jamiespc.y4m
system mkfifo jamiespc.y4m
system rm -f jamiespc.wav
system mkfifo jamiespc.wav

config y4mout output jamiespc.y4m
config wo output jamiespc.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example jamiespc.wav jamiespc.y4m -A 20 -v 4 -f 15 -F 1 | tee cap-jamiespc.ogv | oggfwd 66.228.32.219 8000 bluebutton0123 /jamiespc.ogv &

config vc device 046d
config vc format YUYV
config vc size 640x360
config vc fps 15
config vc autoexpose 3

# Connect everything up
#connect vc Jpeg_buffer jt
#connect jt Jpeg_buffer dj
#connect dj RGB3_buffer cc
#connect dj GRAY_buffer md
#connect cc RGB3_buffer y4mout
#connect md Config_msg cc

connect vc YUV422P_buffer y4mout
connect ac Wav_buffer wo

# Enable vc to start the whole thing running.
config vc enable 1
config ac enable 1
