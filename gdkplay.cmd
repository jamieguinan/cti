new GdkCapture gc
new SDLstuff sdl
new VFilter vf

config vf bottom_crop 60
config sdl mode SOFTWARE
config gc filename $FILENAME
connect gc RGB3_buffer vf

connect vf RGB3_buffer sdl

sdl
