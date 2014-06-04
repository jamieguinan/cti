new Y4MInput y4min
new Y4MOutput y4mout

connect y4min YUV420P_buffer y4mout

config y4mout fps_nom 15
config y4mout fps_denom 1
config y4mout output | x264 /dev/stdin -o test001-output.h264

config y4min input test001-input.y4m
config y4min enable 1
