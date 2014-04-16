new V4L2Capture vc
new SDLstuff sdl
new DO511 do511

connect vc O511_buffer do511
connect do511 RGB3_buffer sdl

config do511 gspca_mode 1
config sdl label BackDoor

config vc device gspca
config vc format O511
config vc size 640x480
config vc contrast 200
config vc saturation 175
config vc enable 1

system sleep 1
