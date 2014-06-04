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
