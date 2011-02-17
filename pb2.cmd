new V4L2Capture vc
new SDLstuff sdl
connect vc BGR3_buffer sdl
config vc device /dev/video0
config vc format BGR3
config vc std NTSC-M
config vc size 640x480
config vc input Television
config vc mute 0
config vc enable 1
