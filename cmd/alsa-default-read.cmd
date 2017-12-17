new ALSACapture ac
new WavOutput wo
config wo output test.wav
connect ac Wav_buffer wo
config ac device default
config ac rate 44100
config ac channels 2
config ac format signed.16-bit.little.endian
config ac enable 1

