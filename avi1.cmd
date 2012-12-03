new AVIDemux ad
new SDLstuff sdl
#new ALSAPlayback ap
new DJpeg dj

connect ad Jpeg_buffer dj
connect dj RGB3_buffer sdl
#connect dv Wav_buffer ap

config sdl mode OVERLAY
#config sdl mode SOFTWARE

#connect ap Feedback_buffer dv
#connect sdl Config_msg mjd

connect sdl:Keycode_msg sdl:Keycode_msg
#connect sdl:Keycode_msg_2 dv:Keycode_msg

config ad input %input
# config ad use_feedback 1

#config ap useplug 1
#config ap device M66
#config ap device Dummy
#config ap device CK804
#config ap device UA25
# rate and channels will be set by first Wav_buffer received.
# 
#config ap format signed.16-bit.little.endian
#config ap frames_per_io 128

#config ap enable 1
config ad enable 1
