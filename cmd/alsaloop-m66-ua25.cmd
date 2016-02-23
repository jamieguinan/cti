# This plays a specific video, with the sound turned off,
# while recording "rec.wav".  The intent is to edit the
# audio into the video.
new ALSACapture ac
new ALSAPlayback ap
new Splitter sp
new WavOutput wavout

connect ac Wav_buffer sp
connect sp:Wav_buffer.1 ap:Wav_buffer
connect sp:Wav_buffer.2 wavout:Wav_buffer

config wavout output rec.wav

config ac useplug 1
config ac device M66
config ac rate 44100
config ac channels 2
config ac frames_per_io 64
config ac format signed.16-bit.little.endian

config ap device UA25
config ap rate 44100
config ap channels 2
config ap frames_per_io 64
config ap format signed.16-bit.little.endian

config ac enable 1
config ap enable 1

system mplayer -nosound -xy 800 /home/guinan/tmp/IMG_0954.MOV
exit
