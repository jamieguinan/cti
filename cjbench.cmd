# This is intended to replace "cjbench.c"

new JpegSource js
new Y4MSource ys
new DJpeg dj
new CJpeg cj
new Null null 

connect js Jpeg_buffer dj
connect dj YUV422P_buffer ys
connect ys YUV422P_buffer cj
connect cj Jpeg_buffer null

# This signals "quit" when done.
connect ys Config_msg dj

config cj time_limit 1.0
config js file ./x1.jpg
config js run 1
system sleep 0.25
config ys run 500
