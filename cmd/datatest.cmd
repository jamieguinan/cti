new RawSource rs
config rs input sat1w:11301

new SubProc submplayer
config submplayer proc mplayer -really-quiet -
#config submplayer cleanup /pkg/misc/mplayer_cleanup
#config submplayer token_channel /tmp/mplayer.cmd

connect rs RawData_buffer mplayer
