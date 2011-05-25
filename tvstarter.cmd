ignoreeof
new Spawn spawn
config spawn cmdline /home/guinan/bin/cti.tv
config spawn trigger_key POWER
new Lirc lirc
connect lirc Keycode_message spawn
