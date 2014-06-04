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
