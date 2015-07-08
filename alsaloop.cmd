new ALSACapture ac
new ALSAPlayback ap

connect ac Wav_buffer ap

config ac device plughw:0
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian

config ap device plughw:0
config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian

config ac enable 1
config ap enable 1
