# Usage: 
#    $ rl cti y422p_to_jpeg.cmd input=file.y422p
#    cti> exit
#    $ mv 000000000.jpg file.jpg

new ImageLoader il
new CJpeg cj
new JpegFiler jf
new PGMFiler pf

connect il 422P_buffer cj
connect cj Jpeg_buffer jf
connect il GRAY_buffer pf

config cj quality 95
config il file %input
