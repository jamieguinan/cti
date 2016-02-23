load /home/guinan/projects/platforms/source/cti/XTion.so
new XTion xt
config xt delayms 33
# config xt file /home/guinan/OpenNI-Linux-x64-2.2/Samples/XTionEvent/xtion-20141125-153031.mjx
config xt file /home/guinan/tmp/xtion-20141125-153031.mjx

new SDLstuff sdl

connect xt GRAY_buffer sdl
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 xt:Keycode_msg
