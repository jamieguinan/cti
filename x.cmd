new MjpegMux mjm
new ALSACapture ac

connect ac Wav_buffer mjm

config mjm output cap.mjx

config ac device hw:3
config ac rate 16000
config ac channels 1
config ac format signed.16-bit.little.endian

config ac enable 1
