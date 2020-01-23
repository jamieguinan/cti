new JpegSource js
new DJpeg dj
new Null null 

config dj dct_method ifast
config js file cmd/x1.jpg
connect js Jpeg_buffer dj
connect dj YUV422P_buffer null
connect js Config_msg dj

# Don't want extra images floating around in the pipeline.
g_synchronous

config js run 100

# Connect to a CJpeg->JpegFiler chain to verify that
# everything is working.
new CJpeg cj
new JpegFiler jf
config jf prefix cmd/
connect dj YUV422P_buffer cj
connect cj Jpeg_buffer jf
config js run 1

