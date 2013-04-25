# Usage: 
#    $ rl cti y422p_to_jpeg.cmd input=file.y422p
#    cti> exit
#    $ mv 000000000.jpg file.jpg

new ImageLoader il
new CJpeg cj
new JpegFiler jf

connect il 422P_buffer cj
connect cj Jpeg_buffer jf

config cj quality 95
config il file %input
