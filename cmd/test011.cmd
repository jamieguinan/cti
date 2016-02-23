new Y4MInput y4min
new H264 venc
new AAC aenc

config venc preset faster
config venc tune zerolatency
config venc profile baseline
config venc output test011-output.264
# exit the program after 10 seconds at 15fps
config venc max_frames 150
# keyint at 5 seconds
config venc keyint_max 75

config venc cqp 25
#config venc crf 20

connect y4min YUV420P_buffer venc

config y4min input test001-input.y4m


config y4min enable 1
