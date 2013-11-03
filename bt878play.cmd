new V4L2Capture vc
new SDLstuff sdl

connect vc BGR3_buffer sdl
connect sdl:Keycode_msg_2 sdl:Keycode_msg

config sdl mode SOFTWARE

config vc device BT878
config vc format BGR3
config vc std NTSC
config vc size 640x480
config vc input S-Video
config vc mute 0
#config vc Contrast 63
config vc enable 1

