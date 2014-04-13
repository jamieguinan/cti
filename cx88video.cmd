# See cx88play.cmd for both audio+video.

new V4L2Capture vc
new SDLstuff sdl

connect vc BGR3_buffer sdl
connect sdl:Keycode_msg sdl:Keycode_msg

#config sdl mode GL
#config sdl mode OVERLAY
config sdl mode SOFTWARE

config vc device cx88
config vc format BGR3
config vc size 640x480
config vc input Composite1
config vc mute 0
config vc Contrast 63
config vc enable 1

#system sleep 3
#Zoom SDL viewport, this actually works!
#config sdl width 1280
#config sdl height 960
