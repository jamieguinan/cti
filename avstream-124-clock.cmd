new MjpegDemux mjd
new Y4MOutput y4mout
new JpegTran jt
new DJpeg dj
new CairoContext cc

# Basic cairo setup.
config cc width 100
config cc height 100
config cc command set_line_width 2.0

# Draw subsecond marker.
config cc command set_source_rgb 0.9 0.9 0.9
config cc command identity_matrix
config cc command translate 50.0 50.0
config cc command rotate_subseconds
config cc command move_to -2.0 -30.0
config cc command rel_line_to 0.0 4.0
config cc command rel_line_to 4.0 0.0
config cc command rel_line_to 0.0 -4.0
config cc command close_path
config cc command fill


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

connect mjd Jpeg_buffer jt
connect jt Jpeg_buffer dj
connect dj 422P_buffer y4mout

config mjd use_feedback 0
config mjd input 192.168.2.124:6666
#config y4mout fps_nom 29.8
config y4mout fps_nom 15
config y4mout fps_denom 1
config y4mout output |ffmpeg2theora - -V 150 -o /dev/stdout | oggfwd localhost 8000 hackme /frontyard.ogv
#config y4mout output out-%s.y4m

config mjd enable 1
