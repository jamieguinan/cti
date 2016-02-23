new TV tv
new Lirc lirc
new ALSAMixer am

config am card M66
config am volume 20

connect lirc Keycode_message tv
connect tv:Mixer_Config_msg am:Config_msg
