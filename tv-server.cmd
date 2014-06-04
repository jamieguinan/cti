new V4L2Capture vc
new ALSACapture ac
new Y4MOutput y4mout
new WavOutput wo

connect vc BGR3_buffer y4mout
connect ac Wav_buffer wo

config y4mout fps_nom 30000
config y4mout fps_denom 1001

system rm -f fifo.y4m
system mkfifo fifo.y4m
system rm -f fifo.wav
system mkfifo fifo.wav

config y4mout output fifo.y4m
config wo output fifo.wav

system ../libtheora-1.2.0alpha1/examples/encoder_example fifo.wav fifo.y4m -a 1 -V 120 --soft-target -z 3 -f 30000 -F 1001 | tee cap.ogv | oggfwd localhost 8000 hackme /tv.ogv &

# -v 0
# -z 0

#config ac device CX8801
#config ac rate 48000
#config ac channels 2

config ac device plughw:3
config ac rate 8000
config ac channels 2

config ac format signed.16-bit.little.endian
config ac enable 1

config vc device cx88
config vc format BGR3
#config vc size 320x240
config vc size 512x384
#config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

#system sleep 60
#abort
