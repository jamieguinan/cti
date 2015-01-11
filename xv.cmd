new SDLstuff sdl
new ImageLoader il
new DJpeg dj

connect sdl:Keycode_msg sdl:Keycode_msg
connect il RGB3_buffer sdl
connect il Jpeg_buffer dj
connect dj RGB3_buffer sdl

config il file %img
