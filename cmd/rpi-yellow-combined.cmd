system ./reset-logitech

new V4L2Capture vc1
new V4L2Capture vc2
new V4L2Capture vc3
new DJpeg dj1
new DJpeg dj2
new DJpeg dj3
new ALSACapture ac
new RPiH264Enc enc
new AAC aenc
new MpegTSMux tsm
new UDPTransmit ut
new Null null
new Compositor comp
new SocketServer ss
new RGB3Source rgb
new CairoContext cc

connect enc H264_buffer tsm
connect ac Wav_buffer aenc
connect aenc AAC_buffer tsm

connect vc1 Jpeg_buffer dj1
connect dj1 YUV420P_buffer comp

connect vc2 Jpeg_buffer dj2
connect dj2 YUV420P_buffer comp

connect vc3 Jpeg_buffer dj3
connect dj3 YUV420P_buffer comp

connect comp YUV420P_buffer enc

connect rgb RGB3_buffer cc
connect cc YUV420P_buffer comp

#config tsm debug_outpackets 1
config tsm pmt_pcrpid 258
config tsm pmt_essd 0:15:257
config tsm pmt_essd 1:27:258

connect tsm RawData_buffer ut
#connect tsm:RawData_buffer_2 ss:RawData_buffer

config vc1 device /dev/v4l/by-id/usb-046d_081b_A53CED50-video-index0
config vc2 device /dev/v4l/by-id/usb-046d_081b_EC8EA150-video-index0
config vc3 device /dev/v4l/by-id/usb-046d_081b_CCBEA150-video-index0

config vc1 label South
config vc2 label West
config vc3 label North

config vc1 format MJPG
config vc1 size 640x360
config vc1 autoexpose 3

config vc2 format MJPG
config vc2 size 640x360
config vc2 autoexpose 3

config vc3 format MJPG
config vc3 size 640x360
config vc3 autoexpose 3

config rgb width 400
config rgb height 280
config rgb color 0x404040

config cc width 400
config cc height 280
config cc label Top
config cc command set_line_width 2.0
config cc command set_source_rgb 0.9 0.9 0.9
config cc command identity_matrix
config cc command set_font_size 20.0
config cc command move_to 120 90
config cc command system_text hostname
config cc command move_to 60 180
config cc command system_text datetime

# Frame rates and periods grouped together here to make it
# easier to keep them in sync.
config vc1 fps 30
config vc2 fps 30
config vc3 fps 30
config enc fps 30
config rgb period_ms 31

config comp size 1120x640
config comp require Top,South,North,West
config comp paste Top:0,0,400,280:360,0:0
config comp paste South:0,0,640,360:0,640:270
config comp paste West:120,0,400,360:760,640:180
config comp paste North:0,0,640,360:760,640:270

config enc bitrate 1500000
config enc gop_seconds 3

config ac device U0x46d0x81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
config ac frames_per_io 256

config ut port 6679
config ut addr 192.168.1.14
#config ut addr 224.0.0.1
#config ut logging 1
# 7x 188 packets = 1316. Need to stay under 1472.
config ut buffer_level 1316

config ss v4port 6680
config ss enable 1

config vc1 enable 1
config vc2 enable 1
config vc3 enable 1
config rgb enable 1
config ac enable 1

# Increase max pending messages from 100 to ...
mpm 10000

ignoreeof 1
