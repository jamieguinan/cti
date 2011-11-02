new JpegSource js
new DJpeg dj
new Null null 

config dj dct_method ifast
config js file cti/x1.jpg
connect js Jpeg_buffer dj
connect dj 422P_buffer null
connect js Config_msg dj

# v 1
config js run 10

