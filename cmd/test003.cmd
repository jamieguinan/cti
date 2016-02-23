new Y4MInput y4min
new H264 venc

config venc preset medium
config venc tune psnr
config venc profile baseline

config venc output test003-output.264

connect y4min YUV420P_buffer venc

config y4min input test001-input.y4m
config y4min enable 1
