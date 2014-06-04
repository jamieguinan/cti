new RGB3Source rgb
config rgb width 800
config rgb height 400
config rgb color 0x202020

new CairoContext cc
config cc width 800
config cc height 400
config cc command set_line_width 2.0
config cc command set_source_rgb 0.8 0.1 0.1

## TO DO:
## Percentages.
## Named registers A..Z, AA..ZZ, etc.

# Fill bar.
config cc command identity_matrix
#config cc command translate 0.0 0.0
config cc command move_to 100 200
config cc command rel_line_to 600 0
config cc command rel_line_to 0 50
config cc command rel_line_to -600 0
config cc command close_path
config cc command fill

# Outline box.
config cc command set_source_rgb 0.8 0.8 0.8
config cc command set_line_width 3.0
config cc command identity_matrix
#config cc command translate 0.0 0.0
config cc command move_to 100 200
config cc command rel_line_to 600 0
config cc command rel_line_to 0 50
config cc command rel_line_to -600 0
config cc command close_path
config cc command stroke

# Send XML messages set X?
# config cc X value

# Text...
config cc command set_source_rgb 0.9 0.9 0.9
config cc command identity_matrix
#config cc command translate 0.0 0.0
config cc command move_to 110 160
config cc command set_font_size 50.0
config cc command show_text

config cc text Video loading...

new SDLstuff sdl
config sdl mode SOFTWARE
connect sdl:Keycode_msg sdl:Keycode_msg

connect rgb RGB3_buffer cc
connect cc RGB3_buffer sdl

#connect rgb RGB3_buffer sdl

config rgb enable 1

# nc host port | mplayer ...
