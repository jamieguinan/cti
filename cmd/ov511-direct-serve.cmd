new V4L2Capture vc
new Y4MOutput y4mout
new MotionDetect md
new DO511 do

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

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.
config y4mout fps_nom 15
config y4mout fps_denom 1

system rm -f ov511.y4m
system mkfifo ov511.y4m

config y4mout output ov511.y4m

system ../libtheora-1.2.0alpha1/examples/encoder_example ov511.y4m -v 4 -f 15 -F 1 | tee cap-basement.ogv | oggfwd localhost 8000 hackme /basement.ogv &
# ... ov511.wav -A 20 ...

config vc device gspca
config vc format O511
config vc size 640x480
config vc fps 15
#config vc autoexpose 3

# Connect everything up
connect vc O511_buffer do
connect do RGB3_buffer cc
connect do GRAY_buffer md
connect cc RGB3_buffer y4mout
connect md Config_msg cc

# Enable mjd to start the whole thing running.
config vc enable 1
