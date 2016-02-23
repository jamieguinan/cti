new Y4MInput yi
config yi input localhost:5000

new SDLstuff sdl
config sdl mode RGB

new VFilter vf

connect yi YUV420P_buffer vf
connect vf YUV422P_buffer sdl

config yi enable 1

config vf top_crop 548
config vf bottom_crop 106


