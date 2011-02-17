# Pull from lr1, feed yCrCb to ffmpeg2theora, then to oggfwd.
# With motion detection.

new V4L2Capture vc
new Y4MOutput y4mout
new DJpeg dj
new JpegTran jt
new MotionDetect md
new DO511 do

new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

# Draw subsecond marker.
#config cc command set_source_rgb 0.9 0.9 0.9
#config cc command identity_matrix
#config cc command translate 50.0 50.0
#config cc command rotate_subseconds
#config cc command move_to -2.0 -30.0
#config cc command rel_line_to 0.0 4.0
#config cc command rel_line_to 4.0 0.0
#config cc command rel_line_to 0.0 -4.0
#config cc command close_path
#config cc command fill

# Draw hour hand.
config cc command set_source_rgb 0.1 0.1 0.9
config cc command identity_matrix
config cc command translate 50.0 50.0
config cc command rotate_hours
config cc command move_to 0.0 0.0
config cc command rel_line_to -4.0 0.0
config cc command rel_line_to 4.0 -25.0
config cc command rel_line_to 4.0 25.0
config cc command close_path
config cc command fill

# Draw minute hand.
config cc command set_source_rgb 0.1 0.1 0.9
config cc command identity_matrix
config cc command translate 50.0 50.0
config cc command rotate_minutes
config cc command move_to 0.0 0.0
config cc command rel_line_to -3.0 0.0
config cc command rel_line_to 3.0 -35.0
config cc command rel_line_to 3.0 35.0
config cc command close_path
config cc command fill

# Draw second hand.
config cc command set_source_rgb 0.9 0.1 0.1
config cc command identity_matrix
config cc command translate 50.0 50.0
config cc command rotate_seconds
config cc command move_to 0.0 0.0
config cc command rel_line_to -1.0 0.0
config cc command rel_line_to 1.0 -30.0
config cc command rel_line_to 1.0 30.0
config cc command close_path
config cc command fill

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
config y4mout output |ffmpeg2theora - -V 150 -o /dev/stdout 2> /dev/null | oggfwd localhost 8000 hackme /frontyard.ogv

config vc device /dev/video2
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
