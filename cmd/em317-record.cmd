new V4L2Capture vc
new ALSACapture ac
new ALSAPlayback ap
new DJpeg dj
new SDLstuff sdl
new UI001 ui
new MjpegMux mjm

new Splitter sp

# The jpeg/rgb path can be there even if it isn't used.
connect vc Jpeg_buffer sp
connect sp:Jpeg_buffer.1 mjm:Jpeg_buffer
connect sp:Jpeg_buffer.2 dj:Jpeg_buffer

connect dj YUV420P_buffer sdl

# Testing YUYV with the gray path.
#connect vc GRAY_buffer sdl

connect sdl Pointer_event ui

config mjm output em317-%s.mjx

config ui add_button text=Set;width=200;height=150;bgcolor=0xff0000

# Ignore backed up frames.
config dj max_messages 2

config vc device UVC
config vc format MJPG
config vc Exposure,.Auto.Priority 0
#config vc size 640x480
config vc size 800x600

config vc fps 15

config vc autoexpose 3

#config vc autoexpose 1
#config vc exposure 300

connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 vc:Keycode_msg

#config ac device U0x46d0x81b
config ac device Q9000
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian
# Bad performance at 64, 128.  256 seems Ok.
config ac frames_per_io 256

connect ac Wav_buffer sp
connect sp:Wav_buffer.1 mjm:Wav_buffer

#config ap device default
#config ap useplug 1
#config ap rate 16000
#config ap channels 1
#config ap format signed.16-bit.little.endian
#config ap frames_per_io 256

config vc enable 1
config ac enable 1
#config ap enable 1
