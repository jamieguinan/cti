new ALSACapture ac
new Null null

connect ac Wav_buffer null

config ac device plughw:3
config ac rate 32000
config ac channels 2
config ac format signed.16-bit.little.endian

config ac enable 1
