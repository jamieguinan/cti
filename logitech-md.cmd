new V4L2Capture vc
new DJpeg dj
new MjpegLocalBuffer mjb
new MotionDetect md
new MjpegMux mjm
new ALSACapture ac

connect vc Jpeg_buffer dj
connect dj GRAY_buffer md
connect md Config_msg mjb
# connect pass-through to buffer
connect dj Jpeg_buffer mjb

config ac device U0x46d0x81b
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

# Ignore backed up frames.
config dj max_messages 2

config dj every 10

config mjm output logitech-md-%s.mjx

config mjb md_threshold 800000
config mjb forwardbuffer 1

connect ac Wav_buffer mjb

connect md MotionDetect_result mjb

connect mjb Jpeg_buffer mjm
connect mjb Wav_buffer mjm
connect mjb:Config_msg mjm:Config_buffer

config vc device UVC
config vc format MJPG
config vc size 640x480
config vc fps 30

#config vc autoexpose 3
config vc autoexpose 1
config vc exposure 300

config vc enable 1
config ac enable 1
