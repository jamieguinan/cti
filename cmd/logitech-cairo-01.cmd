new V4L2Capture vc
new DJpeg dj
new JpegTran jt
new MotionDetect md
new SDLstuff sdl

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
config cc command set_source_rgba 0.9 0.9 0.9 1.0
config cc command identity_matrix
config cc command move_to 10.0 10.0
config cc command show_text

# Parameters.  Only handles fixed FPS, and I have to make sure the source 
# is set up for the same FPS, but it works.

config vc device /dev/video0
config vc format MJPG
config vc size 640x480
# config vc fps 15
config vc fps 30
config vc autoexpose 3

# Connect everything up
connect vc Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj RGB3_buffer cc
connect dj GRAY_buffer md
connect cc RGB3_buffer sdl
connect md Config_msg cc

# Enable mjd to start the whole thing running.
config vc enable 1
