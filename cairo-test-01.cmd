new MjpegDemux mjd
new DO511 do511
new DJpeg dj
new SDLstuff sdl
new JpegTran jt
new CairoContext cc
new MotionDetect md

config sdl label FrontYard
config sdl mode SOFTWARE

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


connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj RGB3_buffer cc
connect dj GRAY_buffer md
connect md Config_msg cc
connect cc RGB3_buffer sdl
connect sdl Feedback_buffer mjd

config mjd input 192.168.2.123:6666
config mjd enable 1
