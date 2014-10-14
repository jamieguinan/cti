new Y4MInput y4m
new SDLstuff sdl
new Y4MOverlay yo

config yo input FFprintcountdown.yuv
#config yo countdown 30

#connect y4m YUV420P_buffer sdl
connect y4m YUV420P_buffer yo
connect yo YUV420P_buffer sdl

config y4m input /tmp/avout
config y4m enable 1
