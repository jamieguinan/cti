new V4L2Capture vc
new CJpeg cj
new MjpegMux mjm
new SocketServer ss
new ALSACapture ac

#connect vc BGR3_buffer cj
#connect vc YUV422P_buffer cj
connect vc YUV420P_buffer cj

connect cj Jpeg_buffer mjm
connect ac Wav_buffer mjm
connect mjm RawData_buffer ss

config ss v4port 5103
config ss enable 1

config vc device BT878

#config vc format BGR3
#config vc format 422P
config vc format YU12

config vc std NTSC
config vc size 640x480
config vc input Composite1
config vc mute 0
#config vc Contrast 63
config vc enable 1

system amixer sset 'Line' 0%,0% cap
system amixer sset 'Capture' 80%,80% cap

config ac device default
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian
config ac frames_per_io 512
config ac enable 1

# config mjm output dell3000-%Y%m%d-%H%M%S.mjx

ignoreeof 1
